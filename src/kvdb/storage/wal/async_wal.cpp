module kvdb.storage.wal.async_wal;

import std;
import kvdb.logging.log;
import kvdb.core.io.file;
import kvdb.storage.wal.wal_record;

using kvdb::core::coro::Task;
using kvdb::core::io::FileMode;
using kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO;

namespace kvdb::storage {

// --- Public API ---

AsyncWal::AsyncWal(IOUring& ring, const std::filesystem::path& path)
    : ring_(&ring), wal_file_(ring, path.string() + "/wal/kvdb.wal", FileMode::ReadWrite) {
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

Task<bool> AsyncWal::async_replay(const std::function<bool(const WalRecord&)>& handler) {
    // 异步 replay 的实现需要异步地、流式地读取文件。
    // 这需要对 File 类进行扩展或使用更底层的 io_uring 操作。
    // 目前，我们先实现一个基于一次性读取整个文件的版本，适用于不太大的WAL文件。

    // 1. 获取文件大小
    auto file_size = wal_file_.get_size();  // 假设 File 类有 get_size() 方法
    if (file_size == 0) {
        co_return true;  // 文件为空，重放成功
    }

    // 2. 一次性读取整个文件
    std::vector<std::byte> buffer(file_size);
    auto read_result = co_await wal_file_.read(buffer, 0);
    if (!read_result || read_result != file_size) {
        LOG_ERROR()("Failed to read entire WAL file for replay.");
        co_return false;
    }

    // 3. 在内存中处理数据
    std::span<const std::byte> data_span(buffer);
    while (!data_span.empty()) {
        // 读取记录长度
        if (data_span.size() < sizeof(std::uint32_t))
            break;
        std::uint32_t record_size;
        std::memcpy(&record_size, data_span.data(), sizeof(record_size));

        if (data_span.size() < record_size)
            break;

        // 反序列化
        auto record_span = data_span.subspan(0, record_size);
        auto record_result = WalRecord::deserialize(record_span);
        if (!record_result) {
            LOG_ERROR()("Failed to deserialize WAL record during replay: {}",
                        record_result.error());
            co_return false;  // 出现损坏，停止重放
        }

        // 处理记录
        if (!handler(*record_result.value())) {
            LOG_ERROR()("WAL replay handler returned false.");
            co_return false;
        }

        // 移动到下一条记录
        data_span = data_span.subspan(record_size);
    }

    co_return true;
}

std::uint64_t AsyncWal::getLastSequenceNumber() const {
    return sequence_number_.load();
}

void AsyncWal::setCurrentSequenceNumber(std::uint64_t seq) {
    sequence_number_ = seq;
}

}  // namespace kvdb::storage
