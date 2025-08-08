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
      read_offset_(0) {  // 初始化 read_offset_
    LOG_INFO()("AsyncWAL initialized for path: {}", path.string());
}

// --- 异步 API 实现 ---

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
    auto data = record.serialize();
    // 使用 -1 表示追加写入
    auto write_result = co_await wal_file_.write(data, -1);

    if (!write_result) {
        LOG_ERROR()("Async write to WAL file failed.");
        co_return false;
    }

    co_return true;
}

// 使用新的 async_read_next_record 重构 async_replay
Task<bool> AsyncWal::async_replay(const std::function<bool(const WalRecord&)>& handler) {
    read_offset_ = 0;  // 每次重放都从头开始

    while (true) {
        auto record_result = co_await async_read_next_record();

        if (!record_result) {
            if (record_result.error() == "EOF") {
                break;  // 正常到达文件末尾
            }
            LOG_ERROR()("Failed during WAL replay: {}", record_result.error());
            co_return false;  // 发生其他错误
        }

        // 调用处理器
        if (!handler(record_result.value())) {
            LOG_ERROR()("WAL replay handler returned false.");
            co_return false;
        }
    }

    co_return true;
}

// 异步地、逐条读取 WAL 记录
Task<std::expected<WalRecord, std::string>> AsyncWal::async_read_next_record() {
    // 1. 先读取记录的总大小
    std::uint32_t record_size;
    auto size_read_result =
        co_await wal_file_.read(std::as_writable_bytes(std::span{&record_size, 1}), read_offset_);

    // 优先判断读取的字节数，0字节表示文件尾（EOF）
    if (size_read_result == 0) {
        co_return std::unexpected("EOF");
    }

    // 如果读取的字节数不完整，则认为是错误
    if (size_read_result != sizeof(record_size)) {
        co_return std::unexpected("Corrupted WAL: Failed to read record size.");
    }

    // 2. 读取整个记录（根据获取的大小）
    std::vector<std::byte> record_buffer(record_size);
    // 从记录的起始位置开始读取整个记录
    auto record_read_result = co_await wal_file_.read(record_buffer, read_offset_);

    if (!record_read_result || record_read_result != record_size) {
        co_return std::unexpected("Corrupted WAL: Failed to read full record.");
    }

    // 3. 反序列化
    auto record = WalRecord::deserialize(record_buffer);
    if (!record) {
        co_return std::unexpected(record.error());
    }

    // 4. 更新偏移量并返回结果
    read_offset_ += record_size;
    co_return std::move(*record.value());
}

std::uint64_t AsyncWal::getLastSequenceNumber() const {
    return sequence_number_.load();
}

auto AsyncWal::getFormattedContent() -> Task<std::expected<std::vector<std::string>, std::string>> {
    std::vector<std::string> lines;
    std::uint64_t current_offset = read_offset_;
    read_offset_ = 0;  // 重置读取偏移量
    while (true) {
        auto record_result = co_await async_read_next_record();
        if (!record_result) {
            if (record_result.error() == "EOF") {
                break;  // 正常到达文件末尾
            }
            co_return std::unexpected(record_result.error());
        }

        const auto& record = record_result.value();
        lines.push_back(record.toString());
    }
    read_offset_ = current_offset;  // 恢复原来的偏移量
    co_return lines;
}

void AsyncWal::setCurrentSequenceNumber(std::uint64_t seq) {
    sequence_number_ = seq;
}

}  // namespace kvdb::storage