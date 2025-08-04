import std;
import kvdb.core.coro.task;
import kvdb.core.coro.awaiter;
import kvdb.storage.sstable;

// An asynchronous function to read a key from an SSTable.
// 异步函数，用于从SSTable读取键。
kvdb::core::coro::Task<void> readKeyFromSSTable(kvdb::storage::SSTable& sstable,
                                                std::string_view key) {
    std::cout << "Coroutine started, about to read key '" << key << "' from SSTable." << std::endl;
    auto value = co_await kvdb::core::coro::SSTableReaderAwaiter(sstable, key);
    if (value) {
        std::cout << "Value for key '" << key << "': " << *value << std::endl;
    } else {
        std::cout << "Key '" << key << "' not found in SSTable." << std::endl;
    }
}

int main() {
    // For demonstration, we need an SSTable. Let's create a dummy one.
    // 为了演示，我们需要一个SSTable。让我们创建一个虚拟的。

    // You would typically load or build the SSTable here.
    // 通常您会在这里加载或构建SSTable。
    kvdb::storage::SSTable::buildFrom("example.sstable",
                                      {
                                          {"a", "value1"},
                                          {"b", "value2"},
                                          {"c", "value3"}
    });
    kvdb::storage::SSTable sstable;
    sstable.open("example.sstable");
    std::cout << "Starting coroutine test for SSTable reading." << std::endl;
    auto task = readKeyFromSSTable(sstable, "a");
    std::cout << "Coroutine task created." << std::endl;

    // Wait for the coroutine to complete by checking the done() status.
    // 通过检查 done() 状态等待协程完成。
    while (!task.done()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Coroutine test for SSTable reading finished." << std::endl;

    return 0;
}