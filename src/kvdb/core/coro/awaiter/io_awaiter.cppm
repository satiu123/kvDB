export module kvdb.core.coro.awaiter.io_awaiter;

import std;
import kvdb.core.coro.task;

// 导出 ReadAwaiter 类，用于 C++20 协程
export class [[nodiscard]] ReadAwaiter {
  public:
    // 构造函数，初始化 Awaiter
    explicit ReadAwaiter(void* ring, int fd, std::span<std::byte> buffer, std::uint64_t offset)
        : ring_(ring), fd_(fd), buffer_(buffer), offset_(offset) {}

    // 总是返回 false，表示需要挂起
    bool await_ready() const noexcept {
        return false;
    }

    // 挂起协程并提交IO请求
    void await_suspend(std::coroutine_handle<> handle) noexcept;

    // 恢复协程时获取结果
    auto await_resume() const noexcept {
        return result_;
    }

    // 设置 IO 操作的结果
    void set_result(std::int32_t res) {
        result_ = res;
    }

    // 获取协程的句柄
    auto get_handle() const noexcept {
        return handle_;
    }

  private:
    void* ring_;
    int fd_;
    std::span<std::byte> buffer_;
    std::uint64_t offset_;
    std::coroutine_handle<> handle_;
    std::int32_t result_{0};
};

// 导出 WriteAwaiter 类，用于 C++20 协程
export class [[nodiscard]] WriteAwaiter {
  public:
    // 构造函数，初始化 Awaiter
    explicit WriteAwaiter(void* ring, int fd, std::span<const std::byte> buffer,
                          std::uint64_t offset)
        : ring_(ring), fd_(fd), buffer_(buffer), offset_(offset) {}

    // 总是返回 false，表示需要挂起
    bool await_ready() const noexcept {
        return false;
    }

    // 挂起协程并提交IO请求
    void await_suspend(std::coroutine_handle<> handle) noexcept;

    // 恢复协程时获取结果
    auto await_resume() const noexcept {
        return result_;
    }

    // 设置 IO 操作的结果
    void set_result(std::int32_t res) {
        result_ = res;
    }

    // 获取协程的句柄
    auto get_handle() const noexcept {
        return handle_;
    }

  private:
    void* ring_;
    int fd_;
    std::span<const std::byte> buffer_;
    std::uint64_t offset_;
    std::coroutine_handle<> handle_;
    std::int32_t result_{0};
};
