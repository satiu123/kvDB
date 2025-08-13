export module kvdb.core.coro.awaiter.io_awaiter;

import std;
import kvdb.core.coro.task;
import kvdb.core.types;

// 导出一个基类 Awaiter，包含通用功能
export class BaseAwaiter {
  public:
    explicit BaseAwaiter(void* ring) : ring_(ring) {}
    virtual ~BaseAwaiter() = default;

    // 总是返回 false，表示需要挂起
    bool await_ready() const noexcept {
        return false;
    }

    void set_result(std::int32_t res) {
        result_ = res;
    }
    auto get_handle() const noexcept {
        return handle_;
    }

  protected:
    void* ring_;
    std::coroutine_handle<> handle_;
    std::int32_t result_{0};
};

// 导出 ReadAwaiter 类，用于 C++20 协程
export class [[nodiscard]] ReadAwaiter : public BaseAwaiter {
  public:
    // 构造函数，初始化 Awaiter
    explicit ReadAwaiter(void* ring, int fd, kvdb::core::types::ByteSpan buffer,
                         std::uint64_t offset)
        : BaseAwaiter(ring), fd_(fd), buffer_(buffer), offset_(offset) {}

    // 挂起协程并提交IO请求
    void await_suspend(std::coroutine_handle<> handle) noexcept;

    // 恢复协程时获取结果
    auto await_resume() const noexcept {
        return result_;
    }

  private:
    int fd_;
    kvdb::core::types::ByteSpan buffer_;
    std::uint64_t offset_;
};

// 导出 WriteAwaiter 类，用于 C++20 协程
export class [[nodiscard]] WriteAwaiter : public BaseAwaiter {
  public:
    // 构造函数，初始化 Awaiter
    explicit WriteAwaiter(void* ring, int fd, kvdb::core::types::ConstByteSpan buffer,
                          std::uint64_t offset)
        : BaseAwaiter(ring), fd_(fd), buffer_(buffer), offset_(offset) {}

    // 挂起协程并提交IO请求
    void await_suspend(std::coroutine_handle<> handle) noexcept;

    // 恢复协程时获取结果
    auto await_resume() const noexcept {
        return result_;
    }

  private:
    int fd_;
    kvdb::core::types::ConstByteSpan buffer_;
    std::uint64_t offset_;
};

// 导出 NopAwaiter，用于无操作的异步等待
export class [[nodiscard]] NopAwaiter : public BaseAwaiter {
  public:
    explicit NopAwaiter(void* ring) : BaseAwaiter(ring) {}

    void await_suspend(std::coroutine_handle<> handle) noexcept;

    void await_resume() const noexcept {}
};
