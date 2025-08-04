# 线程池 (`ThreadPool`) 设计文档

本文档详细介绍了 `kvDB` 项目中通用线程池 (`ThreadPool`) 的设计理念、实现细节和使用方法。

## 1. 设计目标

线程池的主要目标是提供一个高效、健壮的并发任务执行框架。它旨在：

-   **解耦任务提交与执行**：允许主线程异步地提交任务，而无需关心任务在哪个线程中执行。
-   **重用线程资源**：通过复用一组固定的工作线程来避免频繁创建和销毁线程带来的开销。
-   **控制并发级别**：限制并发执行的任务数量，防止系统资源被过度消耗。
-   **提供优雅的关闭机制**：确保在程序退出时，所有已提交的任务都能被完整执行。
-   **支持现代 C++ 特性**：充分利用 C++20/23 的特性，如模块、`std::jthread`、`std::stop_token` 和完美转发，以实现更安全、更简洁的代码。

## 2. 核心组件与实现细节

线程池的核心实现位于 `src/utils/thread_pool.cppm` 中，主要由以下几个部分组成：

### 2.1. `ThreadPool` 类

这是线程池的主类，封装了所有功能。

-   **`workers` (`std::vector<std::jthread>`)**:
    -   这是工作线程的容器。我们选择 `std::jthread` 而不是 `std::thread`，因为它提供了自动的生命周期管理（析构时会自动 `join()`)和内置的协作式中断机制 (`std::stop_token`)。
    -   在构造函数中，会根据用户指定的数量（默认为硬件并发核心数）创建工作线程。

-   **`tasks` (`std::queue<std::function<void()>>`)**:
    -   一个先进先出 (FIFO) 的队列，用于存储等待执行的任务。
    -   任务被包装成 `std::function<void()>`，这使得线程池可以接受任何可调用对象（函数指针、lambda 表达式、成员函数等）。

-   **`queue_mutex` (`std::mutex`)**:
    -   一个互斥锁，用于保护对任务队列 `tasks` 的并发访问，防止多个线程同时修改队列导致的数据竞争。

-   **`condition` (`std::condition_variable_any`)**:
    -   一个条件变量，用于在工作线程和任务提交者之间进行同步。
    -   当任务队列为空时，工作线程会在此条件变量上等待，避免空转浪费 CPU。
    -   当新任务被提交时，提交者会通过此条件变量唤醒一个等待的线程。
    -   我们使用 `std::condition_variable_any` 是因为它能与 `std::jthread` 的 `std::stop_token` 良好地集成。

### 2.2. 构造与析构

-   **构造函数 `ThreadPool(std::size_t threads)`**:
    -   接收一个参数来指定线程数量。
    -   循环创建指定数量的 `std::jthread`，每个线程都执行 `worker_loop` 成员函数。

-   **析构函数 `~ThreadPool()`**:
    -   这是确保线程池优雅关闭的关键。
    -   **步骤 1: 请求停止**：遍历所有 `jthread` 并调用 `request_stop()`。这会设置与每个线程关联的 `stop_token` 的状态。
    -   **步骤 2: 唤醒所有线程**：调用 `condition.notify_all()`。这至关重要，因为它能唤醒所有可能因任务队列为空而正在 `condition.wait()` 中阻塞的线程。被唤醒的线程可以立即检查到停止请求。
    -   **步骤 3: 等待线程完成**：显式地 `join()` 所有工作线程。这会阻塞析构函数，直到所有线程的 `worker_loop` 都已返回。由于 `worker_loop` 的设计保证了只有在所有任务都完成后才会退出，因此这一步确保了任务的完整执行。

### 2.3. 任务提交 (`enqueue`)

-   **`enqueue(F&& f, Args&&... args)`**:
    -   这是一个模板函数，接受一个可调用对象 `f` 和它的参数 `args`。
    -   **完美转发**: 使用 `std::forward` 来完美转发参数，确保参数的左值/右值属性被保留，从而能够高效地处理移动语义。
    -   **任务打包**: 使用 `std::packaged_task` 将函数及其返回值包装起来。这使得我们可以从任务中获取一个 `std::future`，调用者可以用它来等待任务完成并获取结果。
    -   **类型擦除**: 将打包好的任务再次包装进一个 `std::function<void()>` 中，并存入 `tasks` 队列。
    -   **线程安全**: 使用 `std::scoped_lock` 来保护对 `tasks` 队列的访问。
    -   **唤醒线程**: 在任务入队后，调用 `condition.notify_one()` 来唤醒一个可能正在等待的工作线程来执行新任务。

### 2.4. 工作线程循环 (`worker_loop`)

这是每个工作线程执行的核心逻辑。

-   **无限循环**: `while (true)` 循环使线程保持活动状态，不断地从队列中获取并执行任务。
-   **等待任务**:
    -   `std::unique_lock` 和 `condition.wait()` 被用来实现高效的等待。
    -   `wait` 的第三个参数是一个 lambda 谓词：`[this, &st] { return !this->tasks.empty() || st.stop_requested(); }`。
    -   这个谓词是关键：它告诉 `wait` 只有在“任务队列不为空”或“收到了停止请求”时才停止等待。这避免了虚假唤醒（spurious wakeups）导致的问题，并能响应关闭请求。
-   **退出条件**:
    -   在从 `wait` 返回后，线程会检查 `if (st.stop_requested() && this->tasks.empty())`。
    -   这个条件是线程退出的唯一出口。它确保了即使收到了停止请求，线程也会继续处理队列中剩余的任务，直到队列为空，才会最终退出循环。这是保证任务不丢失的核心。
-   **任务执行**:
    -   如果线程没有退出，它会从队列中取出一个任务 (`tasks.front()`, `tasks.pop()`)，然后在释放锁之后执行它。在锁外执行任务是一个重要的优化，它避免了任务执行期间阻塞其他线程访问任务队列。

## 3. 如何使用

```cpp
#include <iostream>
#include <string>
#include <chrono>

// 导入线程池模块
import utils.thread_pool;

int main() {
    // 创建一个拥有 4 个工作线程的线程池
    ThreadPool pool(4);

    // 1. 提交一个无参数、无返回值的任务
    pool.enqueue([] {
        std::cout << "任务 1: 一个简单的 lambda 表达式" << std::endl;
    });

    // 2. 提交一个带参数的任务，并获取 future 来等待结果
    auto future_result = pool.enqueue([](const std::string& name) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return "你好, " + name + "!";
    }, "世界");

    // 3. 主线程可以做其他事情...
    std::cout << "主线程：已提交任务，正在等待结果..." << std::endl;

    // 4. 通过 future.get() 等待任务完成并获取返回值
    //    get() 会阻塞直到任务完成
    std::string result = future_result.get();
    std::cout << "主线程：收到结果 -> " << result << std::endl;

    // 5. 线程池在 main 函数结束时自动析构。
    //    析构函数会确保所有已提交但尚未完成的任务都被执行完毕。
    std::cout << "主线程：即将退出，线程池将自动关闭。" << std::endl;
    
    return 0;
} // pool 在此被析构，所有剩余任务会被处理
```

## 4. 总结

该线程池实现是一个现代、健壮且高效的 C++ 并发工具。它通过 `std::jthread` 和 `std::stop_token` 简化了生命周期管理和关闭流程，并通过条件变量和互斥锁保证了线程安全和高效的任务同步。其设计确保了在任何情况下，所有被成功提交的任务都会被执行，不会因程序关闭而丢失。
