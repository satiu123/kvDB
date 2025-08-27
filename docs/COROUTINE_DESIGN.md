# kvDB 协程 (Coroutine) 设计文档

## 1. 设计目标

`kvDB` 的异步模型完全构建于 C++20 协程之上。我们实现了一个轻量级的协程框架 (`kvdb.core.coro`)，旨在达成以下目标：

-   **简化异步编程**：将带有回调的复杂异步逻辑，转化为易于读写的顺序代码风格。
-   **与 `io_uring` 无缝集成**：提供一种机制，让协程可以自然地挂起，等待 `io_uring` 的 I/O 事件完成，然后在事件就绪时恢复执行。
-   **零动态分配**：`Task` 和 `Awaiter` 的设计避免了在协程挂起和恢复过程中的额外堆内存分配，以追求极致性能。
-   **类型安全**：通过 `Task<T>` 模板，异步操作的返回值可以被静态类型检查。

## 2. 核心组件与实现细节

### 2.1. `kvdb::coro::Task<T>`

`Task<T>` 是 `kvDB` 中所有异步操作的统一返回类型。如果一个函数执行 I/O 或其他可能需要等待的操作，它就应该返回一个 `Task`。

-   **核心职责**: `Task` 封装了一个协程句柄 (`std::coroutine_handle`)，代表了一个可暂停和恢复的计算单元。
-   **Promise Type**: `Task` 的实现核心是其内部的 `promise_type` 结构体。这是 C++20 协程的标准要求，它定义了协程的行为：
    -   `get_return_object()`: 创建与协程关联的 `Task` 对象。
    -   `initial_suspend()`: 返回 `std::suspend_always`，意味着协程在启动后会**立即挂起**，将执行权交还给调用者。这允许我们精确控制任务何时开始执行（通过调用 `resume()`）。
    -   `final_suspend()`: 协程结束时挂起，允许调用者安全地销毁协程或获取结果。
    -   `return_value(T value)` / `return_void()`: 存储协程的返回值。
-   **执行模型**:
    -   `AsyncDatabase::run()` 作为顶层的任务执行器，它调用 `task.resume()` 来启动任务。
    -   当任务 `co_await` 一个 I/O 操作时，它会挂起。
    -   `run()` 方法进入一个循环，不断地调用 `ring_->wait_for_completion()` 来等待 `io_uring` 的事件。
    -   当事件发生，`io_uring` 模块会恢复对应的协程句柄，任务得以继续执行。

### 2.2. Awaiter

Awaiter 是连接协程与特定等待事件（如 I/O）的桥梁。任何可以被 `co_await` 的对象都是一个 Awaiter。它必须实现三个方法：

-   `await_ready()`: 如果返回 `true`，表示操作可以立即完成，无需挂起协程。在我们的 I/O Awaiter 中，它总是返回 `false`。
-   `await_suspend(handle)`: 这是挂起点的核心。当协程挂起时，此函数被调用。它的职责是：
    1.  提交一个异步操作（例如，向 `io_uring` 提交一个读请求）。
    2.  将传入的协程句柄 `handle` 与该操作关联起来。
-   `await_resume()`: 当异步操作完成，协程被恢复后，此函数被调用。它的返回值就是 `co_await` 表达式的结果（例如，读取的字节数）。

### 2.3. `ReadAwaiter` / `WriteAwaiter`

这是专为 `io_uring` 设计的 Awaiter。

-   **构造**: `ReadAwaiter(ring, fd, buffer, offset)` 接收 `io_uring` 实例、文件描述符和读写参数。
-   **挂起**: 在 `await_suspend` 中，它会：
    1.  从 `io_uring` 获取一个可用的提交队列条目 (SQE)。
    2.  使用 `io_uring_prep_read` 或 `io_uring_prep_write` 填充这个 SQE。
    3.  最关键的一步：**将协程句柄 `handle` 保存到 SQE 的 `user_data` 字段中**。
    4.  向 `io_uring` 提交该 SQE。
-   **恢复**: 当 `io_uring` 完成这个 I/O 操作后，`AsyncDatabase::run` 循环会收到一个完成队列条目 (CQE)。我们从 CQE 中取出 `user_data`，它就是我们之前保存的协程句柄。然后我们调用该句柄的 `resume()` 方法，协程就从 `co_await` 之后的地方继续执行了。`await_resume` 会返回 I/O 操作的结果码。

## 3. 工作流程与示例

下面是一个简化的 `async_get` 流程，展示了各组件如何协同工作。

```cpp
// 1. 定义一个返回 Task 的异步函数
Task<std::string> async_read_file(File& file) {
    std::vector<std::byte> buffer(4096);

    // 2. co_await 一个 Awaiter
    // ReadAwaiter 会向 io_uring 提交一个读请求，并把当前协程的 handle 存入 user_data
    // 然后当前协程挂起
    int bytes_read = co_await ReadAwaiter(&ring, file.fd(), buffer, 0);

    // 5. 当 io_uring 通知读操作完成时，协程在这里恢复
    //    bytes_read 变量被赋值为 await_resume() 的返回值
    if (bytes_read > 0) {
        co_return std::string(std::bit_cast<char*>(buffer.data()), bytes_read);
    }
    co_return "Error";
}

// 3. 在顶层执行器中运行任务
void run_example() {
    auto my_task = async_read_file(my_file);
    
    // 4. 启动任务，它会执行到第一个 co_await 点然后挂起
    my_task.resume();

    // ... 在此期间，事件循环正在等待 io_uring 的完成事件 ...
    
    // 6. 任务完成后，可以获取结果
    std::string result = my_task.get();
}
```

## 4. 总结

`kvDB` 的协程框架通过 `Task` 和 `Awaiter` 的精心设计，成功地将 C++20 协程的强大功能与 `io_uring` 的极致性能结合起来。它不仅极大地提升了代码的可读性和可维护性，还通过消除不必要的线程切换和内存分配，为数据库的高性能表现奠定了坚实的基础。
