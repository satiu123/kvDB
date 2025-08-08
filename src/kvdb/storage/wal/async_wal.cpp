module kvdb.storage.wal.async_wal;

import std;
import kvdb.logging.log;
import kvdb.core.io.file;
import kvdb.storage.wal.wal_record;
import kvdb.core.coro.task;

using kvdb::core::coro::Task;
using kvdb::core::io::FileMode;
using kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO;

namespace kvdb::storage {

// --- Public API ---

AsyncWal::AsyncWal(IOUring& ring, const std::filesystem::path& path)
    : ring_(&ring),
      wal_file_(ring, path.string() + "/wal/kvdb.wal", FileMode::ReadWrite),
      read_buffer_(READ_BUFFER_SIZE) {  // 初始化缓冲区大小
    if (!std::filesystem::exists(path / "wal")) {
        std::filesystem::create_directories(path / "wal");
    }
    LOG_INFO()("AsyncWAL initialized for path: {}", path.string());
}

// --- 异步写入 API ---

Task<bool> AsyncWal::async_append_put(std::string_view key, std::string_view value) {
    WalRecord record(WalOpType::PUT, key, value, ++sequence_number_);
    co_return co_await async_append_record(record);
}

Task<bool> AsyncWal::async_append_remove(std::string_view key) {
    WalRecord record(WalOpType::REMOVE, key, "", ++sequence_number_);
    co_return co_await async_append_record(record);
}

Task<bool> AsyncWal::async_append_clear() {
    WalRecord record(WalOpType::CLEAR, "", "", ++sequence_number_);
    co_return co_await async_append_record(record);
}

Task<bool> AsyncWal::async_append_record(const WalRecord& record) {
    const auto required_size = record.size();
    std::vector<std::byte> temp_buffer(required_size);

    auto serialize_result = record.serialize_to(temp_buffer);
    if (!serialize_result) {
        LOG_ERROR()("Failed to serialize WAL record: {}", serialize_result.error());
        co_return false;
    }

    auto write_result = co_await wal_file_.write(temp_buffer, -1);
    if (!write_result) {
        LOG_ERROR()("Async write to WAL file failed.");
        co_return false;
    }

    co_return true;
}

// --- 异步读取 API ---

Task<bool> AsyncWal::async_replay(const std::function<bool(const WalRecord&)>& handler) {
    file_read_offset_ = 0;
    buffer_pos_ = 0;
    buffer_valid_size_ = 0;

    while (true) {
        auto record_result = co_await async_read_next_record();
        if (!record_result) {
            if (record_result.error() == "EOF")
                break;
            LOG_ERROR()("Failed during WAL replay: {}", record_result.error());
            co_return false;
        }
        if (!handler(*record_result)) {
            LOG_ERROR()("WAL replay handler returned false.");
            co_return false;
        }
    }
    co_return true;
}

Task<std::expected<WalRecord, std::string>> AsyncWal::async_read_next_record() {
    // 1. 检查缓冲区是否需要填充
    if (buffer_pos_ + sizeof(std::uint32_t) > buffer_valid_size_) {
        auto fill_res = co_await fill_read_buffer();
        if (!fill_res) {
            co_return std::unexpected(fill_res.error());
        }
        // 如果填充后数据仍然不足，说明到达文件末尾
        if (buffer_pos_ + sizeof(std::uint32_t) > buffer_valid_size_) {
            co_return std::unexpected("EOF");
        }
    }

    // 2. 从缓冲区读取记录头
    std::uint32_t record_size;
    std::memcpy(&record_size, read_buffer_.data() + buffer_pos_, sizeof(record_size));

    // 3. 检查完整记录是否在缓冲区
    if (buffer_pos_ + record_size > buffer_valid_size_) {
        if (record_size > READ_BUFFER_SIZE) {
            co_return std::unexpected("Record size exceeds buffer size.");
        }
        auto fill_res = co_await fill_read_buffer();
        if (!fill_res || buffer_pos_ + record_size > buffer_valid_size_) {
            co_return std::unexpected("Corrupted WAL: partial record at EOF.");
        }
    }

    // 4. 反序列化
    auto record_span = std::span{read_buffer_.data() + buffer_pos_, record_size};
    auto record_result = WalRecord::deserialize(record_span);
    if (!record_result) {
        co_return std::unexpected(record_result.error());
    }

    // 5. 更新状态
    buffer_pos_ += record_size;
    co_return std::move(*record_result);
}

// --- 私有辅助函数 ---

Task<std::expected<std::size_t, std::string>> AsyncWal::fill_read_buffer() {
    // 将未处理的数据移动到缓冲区开头
    if (buffer_pos_ > 0 && buffer_pos_ < buffer_valid_size_) {
        std::memmove(read_buffer_.data(), read_buffer_.data() + buffer_pos_,
                     buffer_valid_size_ - buffer_pos_);
    }
    buffer_valid_size_ -= buffer_pos_;
    buffer_pos_ = 0;

    // 如果缓冲区已满，则无法读取更多数据
    if (buffer_valid_size_ == read_buffer_.size()) {
        co_return 0;
    }

    // 从文件填充缓冲区的剩余空间
    std::span<std::byte> write_span(read_buffer_.data() + buffer_valid_size_,
                                    read_buffer_.size() - buffer_valid_size_);
    auto read_res = co_await wal_file_.read(write_span, file_read_offset_);

    if (!read_res) {
        // **核心修复**：如果读取前没有剩余数据，则将读取失败视作正常的EOF
        if (buffer_valid_size_ == 0) {
            co_return 0;  // 返回成功读取0字节
        }
        // 否则，这是一个真实的读取错误
        co_return std::unexpected("Failed to read from WAL file into buffer.");
    }

    // 更新状态
    file_read_offset_ += read_res;
    buffer_valid_size_ += read_res;

    co_return read_res;
}

// --- 其他 API ---

std::uint64_t AsyncWal::getLastSequenceNumber() const {
    return sequence_number_.load();
}

void AsyncWal::setCurrentSequenceNumber(std::uint64_t seq) {
    sequence_number_ = seq;
}

auto AsyncWal::getFormattedContent() -> Task<std::expected<std::vector<std::string>, std::string>> {
    std::vector<std::string> lines;
    auto original_state = std::make_tuple(file_read_offset_, buffer_pos_, buffer_valid_size_);

    file_read_offset_ = 0;
    buffer_pos_ = 0;
    buffer_valid_size_ = 0;

    while (true) {
        auto record_result = co_await async_read_next_record();
        if (!record_result) {
            if (record_result.error() == "EOF")
                break;
            co_return std::unexpected(record_result.error());
        }
        lines.push_back(record_result->toString());
    }

    std::tie(file_read_offset_, buffer_pos_, buffer_valid_size_) = original_state;
    co_return lines;
}

}  // namespace kvdb::storage
