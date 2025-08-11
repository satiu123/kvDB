# kvDB io_uring 封装设计文档

## 1. 设计目标

`io_uring` 是 Linux 提供的最新、最高性能的异步 I/O 接口。`kvDB` 的性能基石就是建立在 `io_uring` 之上。我们没有直接使用原生的 `liburing` C API，而是创建了一个 C++ 封装层 (`kvdb::core::io::IOUring`)，其设计目标如下：

-   **抽象与封装**：提供一个现代 C++23 风格的、面向对象的接口，隐藏 `liburing` 的底层细节和手动内存管理。
-   **生命周期管理**：通过 RAII (资源获取即初始化) 管理 `io_uring` 实例的创建和销毁，确保资源的正确释放。
-   **与协程集成**：作为协程框架和内核 I/O 事件之间的核心桥梁，负责提交 I/O 请求并分发完成事件以恢复正确的协程。
-   **简化提交流程**：提供简单的方法来获取提交队列条目 (SQE)，并处理提交流程。

## 2. 核心组件与实现细节

### 2.1. `kvdb::core::io::File`

这是一个简单的 RAII 封装类，用于管理文件描述符 (`int fd`)。它的主要职责是：

-   在构造时打开文件，在析构时自动关闭文件。
-   为上层代码提供一个清晰的对象化文件表示，而不是裸露的文件描述符。

### 2.2. `kvdb::core::io::IOUring`

这是 `io_uring` 功能的核心封装类，是整个数据库的事件循环引擎。

-   **初始化**:
    -   构造函数 `IOUring(queue_depth)` 接收一个队列深度参数。
    -   内部调用 `io_uring_queue_init()` 来初始化 `io_uring` 实例，创建内核中的提交队列 (Submission Queue, SQ) 和完成队列 (Completion Queue, CQ)。

-   **提交 I/O 请求**:
    -   `get_sqe()`: 此方法调用 `io_uring_get_sqe()`，从提交队列中获取一个可用的条目 (`io_uring_sqe`)。如果队列已满，它会自动提交现有请求以腾出空间。
    -   `submit()`: 调用 `io_uring_submit()` 将所有准备好的 SQE 提交给内核执行。
    -   **工作模式**: `Awaiter` (如 `ReadAwaiter`) 在其 `await_suspend` 方法中，会调用 `get_sqe()`，然后使用 `liburing` 的辅助函数（如 `io_uring_prep_read`）来填充这个 SQE，最后将自身的协程句柄存入 `user_data` 字段。

-   **处理 I/O 完成 (Event Loop)**:
    -   `wait_for_completion()`: 这是 `AsyncDatabase` 中主事件循环调用的核心方法。
    -   **等待事件**: 它首先调用 `io_uring_wait_cqe()`，这个调用会阻塞，直到至少有一个 I/O 操作完成。
    -   **遍历 CQ**: 一旦被唤醒，它会调用 `io_uring_peek_cqe()` 和 `io_uring_cqe_seen()` 在一个循环中遍历所有已完成的队列条目 (CQE)。
    -   **恢复协程**: 对于每一个 CQE，它执行以下关键步骤：
        1.  从 `cqe->user_data` 中提取出之前存入的协程句柄 (`std::coroutine_handle<>`)。
        2.  从 `cqe->res` 中提取 I/O 操作的结果（例如，成功读取的字节数或错误码）。
        3.  通过协程句柄找到对应的 `Awaiter` 对象，并将结果设置给它。
        4.  调用 `handle.resume()`，唤醒等待该 I/O 操作的协程。

## 3. 工作流程

一个完整的异步读操作流程展示了所有组件的协同工作：

1.  **调用**: 上层代码（如 `AsyncSSTable::find`）调用一个返回 `Task` 的函数，该函数内部需要读取文件。
2.  **等待**: 该函数 `co_await` 一个 `ReadAwaiter`。
3.  **挂起与提交**: `ReadAwaiter::await_suspend` 被调用：
    a.  它向 `IOUring` 实例请求一个 SQE (`get_sqe()`)。
    b.  它用读请求参数填充 SQE。
    c.  它将自己的协程句柄存入 `sqe->user_data`。
    d.  `IOUring` 提交该 SQE 到内核。
    e.  `async_read` 协程挂起，执行权返回到 `AsyncDatabase::run` 的事件循环。
4.  **等待与完成**: `AsyncDatabase::run` 调用 `IOUring::wait_for_completion()`，阻塞等待内核的 I/O 完成信号。
5.  **恢复**: 内核完成读操作后，`wait_for_completion()` 被唤醒。它从 CQE 中解析出协程句柄和结果，并调用 `handle.resume()`。
6.  **继续执行**: `async_read` 协程从 `co_await` 处恢复执行，`await_resume` 返回读取的字节数，函数继续执行后续逻辑。

## 4. 总结

`kvdb::core::io::IOUring` 模块是 `kvDB` 高性能异步架构的底层引擎。它通过对 `liburing` 的精心封装，提供了一个健壮、高效且与 C++20 协程无缝集成的接口。这个模块成功地将上层简洁的异步业务逻辑与底层复杂的内核 I/O 事件处理分离开来，是整个项目实现高吞吐、低延迟的关键。
