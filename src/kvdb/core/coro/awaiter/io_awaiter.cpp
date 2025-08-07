module;

#include <coroutine>

module kvdb.core.coro.awaiter.io_awaiter;

import kvdb.core.io.io_uring;

void ReadAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<IOUring*>(ring_)->submit_read_request(fd_, buffer_, offset_,
                                                      reinterpret_cast<std::uint64_t>(this));
}

void WriteAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<IOUring*>(ring_)->submit_write_request(fd_, buffer_, offset_,
                                                       reinterpret_cast<std::uint64_t>(this));
}
