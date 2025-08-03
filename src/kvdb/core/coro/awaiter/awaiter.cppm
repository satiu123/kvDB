export module kvdb.core.coro.awaiter;
export import :file_reader;

import std;

export namespace kvdb::core::coro {

struct Awaiter {
    std::coroutine_handle<> handle_;

    Awaiter(std::coroutine_handle<> handle) : handle_(handle) {}

    bool await_ready() const noexcept {
        return false;  // Always suspend
    }

    void await_suspend(std::coroutine_handle<> /*unused*/) noexcept {
        // Resume the coroutine when the awaitable is ready
        handle_.resume();
    }

    void await_resume() noexcept {
        // No result to return, just resume the coroutine
    }
};
class FileAwaiter {
  public:
    std::coroutine_handle<> handle_;

    FileAwaiter(std::coroutine_handle<> handle) : handle_(handle) {}
    bool await_ready() const noexcept {
        return false;  // Always suspend
    }

    void await_suspend(std::coroutine_handle<> /*unused*/) noexcept {
        // Resume the coroutine when the file operation is ready
        handle_.resume();
    }
    void await_resume() noexcept {
        // No result to return, just resume the coroutine
    }
};
}  // namespace kvdb::core::coro