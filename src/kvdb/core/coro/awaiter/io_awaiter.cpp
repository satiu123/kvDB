module kvdb.core.coro.awaiter.io_awaiter;
import std;
import kvdb.core.io.ring_api;
using kvdb::core::io::ISubmitter;
void ReadAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<ISubmitter*>(ring_)->submit_read_request(fd_, buffer_, offset_, this);
}

void WriteAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<ISubmitter*>(ring_)->submit_write_request(fd_, buffer_, offset_, this);
}

void NopAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept {
    handle_ = handle;
    static_cast<ISubmitter*>(ring_)->submit_nop_request(this);
}
