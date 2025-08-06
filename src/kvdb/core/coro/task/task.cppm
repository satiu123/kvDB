export module kvdb.core.coro.task;

import std;

// 导出 Task 类，用于 C++20 协程
export template <typename T>
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
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void return_value(T value) {
            this->value = std::move(value);
        }
        void unhandled_exception() {
            std::terminate();
        }

        T value;
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
export template <>
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
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void return_void() {}
        void unhandled_exception() {
            std::terminate();
        }
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
