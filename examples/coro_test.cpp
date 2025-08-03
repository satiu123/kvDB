import std;
import kvdb.core.coro.task;
import kvdb.core.coro.awaiter;

kvdb::core::coro::Task<void> readFileAndPrint() {
    std::cout << "Coroutine started, about to read file." << std::endl;
    auto content = co_await kvdb::core::coro::ReadFile("test_file.txt");
    std::cout << "File content: " << content << std::endl;
}

int main() {
    std::cout << "Starting coroutine test." << std::endl;
    {
        std::jthread coroThread1(readFileAndPrint);
    }
    std::cout << "Coroutine test finished." << std::endl;
    // Since the coroutine runs in a detached thread,
    // we might need to wait a bit for it to finish.
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}