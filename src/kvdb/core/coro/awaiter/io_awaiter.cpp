module kvdb.core.coro.awaiter.io_awaiter;
import std;
import kvdb.core.io.io_uring;
using kvdb::core::io::IOUring;
void ReadAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<IOUring*>(ring_)->submit_read_request(fd_, buffer_, offset_, this);
}

void WriteAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<IOUring*>(ring_)->submit_write_request(fd_, buffer_, offset_, this);
}

void NopAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<IOUring*>(ring_)->submit_nop_request(this);
}
