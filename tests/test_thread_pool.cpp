#include <gtest/gtest.h>

import std;
// 导入我们的线程池模块
import utils.thread_pool;

// --- 测试集 1: 线程池生命周期管理 ---

// 测试线程池的构造和析构是否正常，不会导致挂起
TEST(ThreadPoolLifecycle, ConstructionAndDestruction) {
    utils::ThreadPool pool(4);
    SUCCEED();  // 如果能执行到这里，说明构造和析构没有立即出问题
}  // pool 在此被析构

// 测试线程池在析构时，会等待所有待处理的任务完成
TEST(ThreadPoolLifecycle, ShutdownWithPendingTasks) {
    std::atomic<int> counter = 0;
    const int num_tasks = 10;
    {
        utils::ThreadPool pool(4);
        for (int i = 0; i < num_tasks; ++i) {
            pool.enqueue([&counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                counter++;
            });
        }
        // 不等待 future，直接让 pool 析构
    }  // 析构函数应该会阻塞，直到所有任务完成

    // 验证所有任务都已在析构前执行完毕
    EXPECT_EQ(counter.load(), num_tasks);
}

// --- 测试集 2: 任务执行的各种场景 ---

// 测试从任务中获取各种类型的返回值
TEST(ThreadPoolTasks, ReturnValue) {
    utils::ThreadPool pool(1);
    auto future_int = pool.enqueue([] { return 42; });
    auto future_str = pool.enqueue([] { return std::string("hello"); });

    EXPECT_EQ(future_int.get(), 42);
    EXPECT_EQ(future_str.get(), "hello");
}

// 测试向任务传递不同类型的参数
TEST(ThreadPoolTasks, TaskWithArguments) {
    utils::ThreadPool pool(1);
    auto future = pool.enqueue(
        [](int a, const std::string& b, std::string&& c) { return std::to_string(a) + b + c; }, 1,
        "-text-", "-move");

    EXPECT_EQ(future.get(), "1-text--move");
}

// 测试向任务传递仅移动(move-only)类型的参数
TEST(ThreadPoolTasks, TaskWithMoveOnlyArgument) {
    utils::ThreadPool pool(1);
    auto ptr = std::make_unique<int>(123);

    auto future = pool.enqueue([](std::unique_ptr<int> p) { return *p; }, std::move(ptr));

    EXPECT_EQ(future.get(), 123);
    // 验证原始指针已被移动
    EXPECT_EQ(ptr, nullptr);
}

// 测试当任务抛出异常时，future.get() 会重新抛出该异常
TEST(ThreadPoolTasks, TaskThrowsException) {
    utils::ThreadPool pool(1);
    auto future = pool.enqueue([]() { throw std::runtime_error("Task failed"); });

    // 验证 future.get() 会抛出我们预期的异常
    EXPECT_THROW(
        {
            try {
                future.get();
            } catch (const std::runtime_error& e) {
                EXPECT_STREQ(e.what(), "Task failed");
                throw;  // 重新抛出以满足 EXPECT_THROW
            }
        },
        std::runtime_error);
}

// --- 测试集 3: 并发与压力测试 ---

// 从多个线程并发地向线程池提交任务
TEST(ThreadPoolConcurrency, ConcurrentEnqueue) {
    utils::ThreadPool pool(8);
    std::atomic<int> counter = 0;
    const int num_threads = 4;
    const int tasks_per_thread = 50;

    std::vector<std::thread> submitters;
    for (int i = 0; i < num_threads; ++i) {
        submitters.emplace_back([&pool, &counter]() {
            for (int j = 0; j < tasks_per_thread; ++j) {
                pool.enqueue([&counter] { counter++; });
            }
        });
    }

    // 等待所有提交线程完成
    for (auto& t : submitters) {
        t.join();
    }

    // 为了确保所有任务都已完成，我们创建一个同步点任务
    auto sync_future = pool.enqueue([] {});
    sync_future.get();

    EXPECT_EQ(counter.load(), num_threads * tasks_per_thread);
}

// --- 测试集 4: 边界情况 ---

// 测试创建一个0线程的线程池
TEST(ThreadPoolEdgeCases, ZeroThreads) {
    std::atomic<bool> task_executed = false;
    {
        utils::ThreadPool pool(0);
        auto future = pool.enqueue([&task_executed] { task_executed = true; });

        // 任务不应该被执行，因为没有工作线程
        using namespace std::chrono_literals;
        auto status = future.wait_for(10ms);
        EXPECT_EQ(status, std::future_status::timeout);
        EXPECT_FALSE(task_executed.load());

    }  // 析构不应该挂起

    // 确认任务最终也未被执行
    EXPECT_FALSE(task_executed.load());
}