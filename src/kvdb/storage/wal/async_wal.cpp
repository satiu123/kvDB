module kvdb.storage.wal.async_wal;

import std;
import kvdb.logging.log;
import kvdb.core.io.file;
import kvdb.storage.wal.wal_record;
import kvdb.core.coro.task;
import kvdb.core.types;

using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::FileMode;
using kvdb::core::types::ByteSpan;
using kvdb::core::types::KeyView;
using kvdb::core::types::Result;
using kvdb::core::types::ValueView;
using kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO;

namespace kvdb::storage {

// --- 公共 API ---

AsyncWal::AsyncWal(IOUring& ring, const std::filesystem::path& path)
    : ring_(&ring),
      wal_path_(path / "wal" / "kvdb.wal"),
      wal_file_(ring, wal_path_, FileMode::ReadWrite),
      read_buffer_(READ_BUFFER_SIZE) {  // 初始化缓冲区大小
    if (!std::filesystem::exists(path / "wal")) {
        std::filesystem::create_directories(path / "wal");
    }
    LOG_INFO()("异步WAL已为路径'{}'初始化", path.string());
}

// --- 异步写入 API ---

Task<bool> AsyncWal::async_append_put(KeyView key, ValueView value) {
    WalRecord record(WalOpType::PUT, key, value, ++sequence_number_);
    LOG_DEBUG()("追加PUT记录到WAL: key={}, seq={}", key, sequence_number_.load());
    co_return co_await async_append_record(record);
}

Task<bool> AsyncWal::async_append_remove(KeyView key) {
    WalRecord record(WalOpType::REMOVE, key, "", ++sequence_number_);
    LOG_DEBUG()("追加REMOVE记录到WAL: key={}, seq={}", key, sequence_number_.load());
    co_return co_await async_append_record(record);
}

Task<bool> AsyncWal::async_append_clear() {
    WalRecord record(WalOpType::CLEAR, "", "", ++sequence_number_);
    LOG_DEBUG()("追加CLEAR记录到WAL: seq={}", sequence_number_.load());
    co_return co_await async_append_record(record);
}

Task<bool> AsyncWal::async_append_record(const WalRecord& record) {
    const auto required_size = record.size();
    std::vector<std::byte> temp_buffer(required_size);

    auto serialize_result = record.serialize_to(temp_buffer);
    if (!serialize_result) {
        LOG_ERROR()("序列化WAL记录失败: {}", serialize_result.error());
        co_return false;
    }

    auto write_result = co_await wal_file_.write(temp_buffer, -1);
    if (write_result < 0) {  // 检查小于0的错误码
        LOG_ERROR()("异步写入WAL文件失败，错误码: {}", write_result);
        co_return false;
    }

    co_return true;
}

// --- 异步读取 API ---

Task<bool> AsyncWal::async_replay(const std::function<bool(const WalRecord&)>& handler) {
    LOG_INFO()("开始异步WAL重放...");
    file_read_offset_ = 0;
    buffer_pos_ = 0;
    buffer_valid_size_ = 0;

    while (true) {
        auto record_result = co_await async_read_next_record();
        if (!record_result) {
            if (record_result.error() == "EOF") {
                LOG_DEBUG()("到达WAL文件末尾，重放结束");
                break;
            }
            LOG_ERROR()("WAL重放过程中出错: {}", record_result.error());
            co_return false;
        }
        if (!handler(*record_result)) {
            LOG_ERROR()("WAL重放处理器返回false，重放终止");
            co_return false;
        }
    }
    LOG_INFO()("异步WAL重放成功完成");
    co_return true;
}

Task<Result<WalRecord>> AsyncWal::async_read_next_record() {
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
            LOG_ERROR()("记录大小({})超过缓冲区大小({})", record_size, READ_BUFFER_SIZE);
            co_return std::unexpected("记录大小超过缓冲区大小");
        }
        auto fill_res = co_await fill_read_buffer();
        if (!fill_res || buffer_pos_ + record_size > buffer_valid_size_) {
            LOG_ERROR()("损坏的WAL：在文件末尾发现部分记录");
            co_return std::unexpected("损坏的WAL：在文件末尾发现部分记录");
        }
    }

    // 4. 反序列化
    auto record_span = ByteSpan{read_buffer_}.subspan(buffer_pos_, record_size);
    auto record_result = WalRecord::deserialize(record_span);
    if (!record_result) {
        co_return std::unexpected(record_result.error());
    }

    // 5. 更新状态
    buffer_pos_ += record_size;
    co_return *record_result;
}

// --- 私有辅助函数 ---

Task<Result<std::size_t>> AsyncWal::fill_read_buffer() {
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
    ByteSpan write_span = ByteSpan{read_buffer_}.subspan(buffer_valid_size_);
    auto read_res = co_await wal_file_.read(write_span, file_read_offset_);

    if (read_res < 0) {  // 小于0表示错误
        if (buffer_valid_size_ == 0) {
            co_return 0;  // 如果之前没有数据，认为是正常的EOF
        }
        LOG_ERROR()("从WAL文件向缓冲区读取失败，错误码: {}", read_res);
        co_return std::unexpected("从WAL文件向缓冲区读取失败");
    }

    // 更新状态
    file_read_offset_ += read_res;
    buffer_valid_size_ += read_res;

    co_return static_cast<std::size_t>(read_res);
}

// --- 其他 API ---

std::uint64_t AsyncWal::getLastSequenceNumber() const {
    return sequence_number_.load();
}

void AsyncWal::setCurrentSequenceNumber(std::uint64_t seq) {
    sequence_number_ = seq;
}

void AsyncWal::truncate() {
    // 删除旧 WAL 文件
    File::remove(wal_path_);
    // 用空文件替换，并保持 wal_file_ 为可读写以供后续 append
    File new_file(*ring_, wal_path_, FileMode::ReadWrite);
    wal_file_ = std::move(new_file);
    // 重置读取与序列号状态
    file_read_offset_ = 0;
    buffer_pos_ = 0;
    buffer_valid_size_ = 0;
}

auto AsyncWal::getFormattedContent() -> Task<Result<std::vector<std::string>>> {
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