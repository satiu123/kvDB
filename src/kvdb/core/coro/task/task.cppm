export module kvdb.core.coro.task;
import std;


export namespace kvdb::core::coro {
template <typename T>
class Task {
  public:
    struct promise_type {
        Task<T> get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept {
            return {};
        }

        std::suspend_always final_suspend() noexcept {
            return {};
        }

        template <typename U>
        void return_value(U&& value) {
            result_ = std::forward<U>(value);
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        T& result() {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            return result_;
        }

      private:
        T result_;
        std::exception_ptr exception_;
    };

    explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    T result() {
        return handle_.promise().result();
    }

    bool done() const {
        return handle_.done();
    }

  private:
    std::coroutine_handle<promise_type> handle_;
};

template <>
class Task<void> {
  public:
    struct promise_type {
        Task<void> get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept {
            return {};
        }

        std::suspend_always final_suspend() noexcept {
            return {};
        }

        void return_void() {}

        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        void result() {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
        }

      private:
        std::exception_ptr exception_;
    };

    explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    void result() {
        handle_.promise().result();
    }

    bool done() const {
        return handle_.done();
    }

  private:
    std::coroutine_handle<promise_type> handle_;
};
}  // namespace kvdb::core::coro