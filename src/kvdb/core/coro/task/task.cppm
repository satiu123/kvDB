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
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<>
                await_suspend(std::coroutine_handle<promise_type> h) noexcept {
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
            this->value = std::move(value);
        }
        void unhandled_exception() {
            std::terminate();
        }

        T value;
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

    auto operator co_await() noexcept {
        struct awaiter {
            std::coroutine_handle<promise_type> handle_;

            bool await_ready() const noexcept {
                return !handle_ || handle_.done();
            }

            void await_suspend(std::coroutine_handle<> awaiting_handle) noexcept {
                handle_.promise().continuation = awaiting_handle;
                handle_.resume();
            }

            T await_resume() noexcept {
                return std::move(handle_.promise().value);
            }
        };
        return awaiter{handle_};
    }

    // 获取结果
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
        return std::move(handle_.promise().value);
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
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<>
                await_suspend(std::coroutine_handle<promise_type> h) noexcept {
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
            std::terminate();
        }
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

    auto operator co_await() noexcept {
        struct awaiter {
            std::coroutine_handle<promise_type> handle_;

            bool await_ready() const noexcept {
                return !handle_ || handle_.done();
            }

            void await_suspend(std::coroutine_handle<> awaiting_handle) noexcept {
                handle_.promise().continuation = awaiting_handle;
                handle_.resume();
            }

            void await_resume() noexcept {}
        };
        return awaiter{handle_};
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
    void get() {}

  private:
    std::coroutine_handle<promise_type> handle_;
};
}  // namespace kvdb::core::coro