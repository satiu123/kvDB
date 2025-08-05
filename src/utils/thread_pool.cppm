export module utils.thread_pool;

import std;
export namespace utils {
class ThreadPool {
  public:
    /**
     * @brief 构造函数
     * @param threads 要创建的线程数量
     */
    explicit ThreadPool(std::size_t threads = std::thread::hardware_concurrency()) {
        for (std::size_t i = 0; i < threads; ++i) {
            // 创建 jthread，它会自动管理生命周期
            // 每个 jthread 都会获得一个 stop_token
            workers.emplace_back([this](const std::stop_token& st) { this->worker_loop(st); });
        }
    }

    /**
     * @brief 析构函数
     *        请求所有线程停止，并等待它们完成
     */
    ~ThreadPool() {
        // 1. 显式地请求所有工作线程停止
        for (auto& worker : workers) {
            worker.request_stop();
        }
        // 2. 唤醒所有可能正在等待的线程，以便它们可以检查停止请求
        condition.notify_all();

        // 3. 显式地 join 所有线程，确保它们在析构前完成
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief 向任务队列中添加一个新任务
     * @tparam F 函数类型
     * @tparam Args 参数类型
     * @param f 函数对象
     * @param args 函数参数
     * @return 一个 std::future 对象，可用于获取任务的返回值
     */
    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;
        using task_type = std::packaged_task<return_type()>;

        // 将函数和参数打包，以正确处理移动语义
        // 这种方式比 std::bind_front 更健壮
        auto task_lambda =
            [func = std::forward<F>(f),
             args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable -> return_type {
            return std::apply(func, std::move(args_tuple));
        };

        auto task = std::make_shared<task_type>(std::move(task_lambda));

        std::future<return_type> res = task->get_future();
        {
            std::scoped_lock lock(queue_mutex);

            // 只有当线程池真正开始停止时，才拒绝新任务
            if (!workers.empty() && workers[0].get_stop_source().stop_requested()) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

  private:
    /**
     * @brief 工作线程的主循环
     * @param st 停止令牌，用于协作式停止
     */
    void worker_loop(const std::stop_token& st) {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(queue_mutex);
                // 等待，直到有任务或收到停止请求
                // 关键改动：即使收到停止请求，如果队列不为空，也要继续处理
                condition.wait(lock, st,
                               [this, &st] { return !this->tasks.empty() || st.stop_requested(); });

                // 关键的最终修复：
                // 只有当“停止被请求”并且“任务队列为空”这两个条件同时满足时，线程才应该退出。
                if (st.stop_requested() && this->tasks.empty()) {
                    return;
                }

                // 如果执行到这里，说明队列中一定有任务，取出一个来执行
                task = std::move(tasks.front());
                tasks.pop();
            }

            if (task) {
                task();
            }
        }
    }

    std::vector<std::jthread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable_any condition;
    // 不再需要自己的 stop_source，因为 jthread 会管理自己的停止状态
};
}  // namespace utils