export module kvdb.core.coro.awaiter:sstable_reader_awaiter;

import std;
import kvdb.storage.sstable;
import kvdb.core.coro.task;
using kvdb::storage::SSTable;
namespace kvdb::core::coro {
// 用于从SSTable异步读取键的Awaiter。
export class [[nodiscard]] SSTableReaderAwaiter {
  public:
    SSTableReaderAwaiter(SSTable& sstable, std::string_view key) : sstable_{sstable}, key_{key} {}

    // 总是在等待，因为文件I/O不应阻塞调用者。
    bool await_ready() const noexcept {
        return false;
    }

    // 核心逻辑：在后台线程中执行读取操作。
    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([this, handle] {
            // 在实际场景中，您可能会使用线程池。
            result_ = sstable_.find(key_);
            // 读取数据后，恢复协程。
            handle.resume();
        }).detach();
    }

    // 协程恢复后，返回结果。
    auto await_resume() noexcept -> std::optional<std::string> {
        return result_;
    }

  private:
    SSTable& sstable_;
    std::string key_;
    std::optional<std::string> result_;
};
}  // namespace kvdb::core::coro
