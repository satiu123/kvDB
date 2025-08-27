export module kvdb.core.coro.task;

import std;

export namespace kvdb::core::coro {

// 导出 Task 类，用于 C++20 协程
template <typename T>
class [[nodiscard]] Task {
  public:
    // promise_type 结构体
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        auto final_suspend() noexcept {
            struct awaiter {
                bool await_ready() noexcept {
                    return false;
                }
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept {
                    auto continuation = h.promise().continuation;
                    if (continuation) {
                        return continuation;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return awaiter{};
        }
        void return_value(T value) {
            this->value.emplace(std::move(value));
        }
        void unhandled_exception() {
            exception = std::current_exception();
        }

        std::optional<T> value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation = nullptr;
    };

    // 构造函数
    explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    // 析构函数
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // 禁止拷贝
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 允许移动
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    // 禁止对左值进行 co_await，避免悬挂/重复等待
    auto operator co_await() & = delete;

    // 仅允许右值 co_await：把所有权移交给 awaiter，并在 await_resume 中销毁帧
    auto operator co_await() && noexcept {
        struct awaiter {
            std::coroutine_handle<promise_type> handle_;

            bool await_ready() const noexcept {
                return !handle_ || handle_.done();
            }

            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<> awaiting_handle) noexcept {
                handle_.promise().continuation = awaiting_handle;
                return handle_;
            }

            T await_resume() {
                auto h = handle_;
                handle_ = {};
                auto& p = h.promise();
                // 拿到结果/异常后销毁协程帧
                if (p.exception) {
                    h.destroy();
                    std::rethrow_exception(p.exception);
                }
                T out = std::move(*p.value);
                h.destroy();
                return out;
            }
        };
        auto h = handle_;
        handle_ = {};
        return awaiter{h};
    }

    // 检查协程是否完成
    bool done() const {
        return !handle_ || handle_.done();
    }

    // 恢复协程执行
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    // 获取结果
    T get() {
        if (!handle_) {
            throw std::runtime_error("Task has no coroutine handle");
        }
        if (!handle_.done()) {
            handle_.resume();
        }
        auto h = handle_;
        handle_ = {};
        auto& p = h.promise();
        if (p.exception) {
            h.destroy();
            std::rethrow_exception(p.exception);
        }
        T out = std::move(*p.value);
        h.destroy();
        return out;
    }

  private:
    std::coroutine_handle<promise_type> handle_;
};

// Task<void> 的特化
template <>
class [[nodiscard]] Task<void> {
  public:
    // promise_type 结构体
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        auto final_suspend() noexcept {
            struct awaiter {
                bool await_ready() noexcept {
                    return false;
                }
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept {
                    auto continuation = h.promise().continuation;
                    if (continuation) {
                        return continuation;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return awaiter{};
        }
        void return_void() {}
        void unhandled_exception() {
            exception = std::current_exception();
        }
        std::exception_ptr exception;
        std::coroutine_handle<> continuation = nullptr;
    };

    // 构造函数
    explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    // 析构函数
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // 禁止拷贝
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 允许移动
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    // 禁止对左值进行 co_await
    auto operator co_await() & = delete;

    // 仅允许右值 co_await
    auto operator co_await() && noexcept {
        struct awaiter {
            std::coroutine_handle<promise_type> handle_;

            bool await_ready() const noexcept {
                return !handle_ || handle_.done();
            }

            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<> awaiting_handle) noexcept {
                handle_.promise().continuation = awaiting_handle;
                return handle_;
            }

            void await_resume() {
                auto h = handle_;
                handle_ = {};
                auto& p = h.promise();
                if (p.exception) {
                    h.destroy();
                    std::rethrow_exception(p.exception);
                }
                h.destroy();
            }
        };
        auto h = handle_;
        handle_ = {};
        return awaiter{h};
    }

    // 检查协程是否完成
    bool done() const {
        return !handle_ || handle_.done();
    }

    // 恢复协程执行
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    // 获取结果 (void)
    void get() {
        if (!handle_) {
            return;
        }
        if (!handle_.done()) {
            handle_.resume();
        }
        auto h = handle_;
        handle_ = {};
        auto& p = h.promise();
        if (p.exception) {
            h.destroy();
            std::rethrow_exception(p.exception);
        }
        h.destroy();
    }

  private:
    std::coroutine_handle<promise_type> handle_;
};
}  // namespace kvdb::core::coro
