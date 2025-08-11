# kvDB 日志系统设计文档 

## 1. 设计目标

`kvdb::logging` 模块为整个项目提供了一个高性能、线程安全且可扩展的日志记录框架。其核心设计目标如下：

-   **高性能与低延迟**：日志的产生（格式化字符串）和写入（I/O 操作）是解耦的。调用日志接口时，仅将一条结构化的 `LogRecord` 推入内存队列，然后立即返回，对业务线程的影响降到最低。
-   **线程安全**：日志系统内部处理了所有同步问题，允许多个线程在任何时候安全地调用日志接口。
-   **多级日志**：支持 `DEBUG`, `INFO`, `WARNING`, `ERROR` 等多个日志级别，并可以动态设置全局日志级别。
-   **可扩展的输出 (Sinks)**：可以同时将日志输出到多个目的地（如控制台、文件），并且可以轻松添加新的输出类型（如网络、数据库等）。
-   **易用性**: 提供 `LOG_INFO()("message: {}", value)` 这样简单直观的函数式接口，自动捕获源码位置，并支持 `std::format` 语法。

## 2. 核心组件与架构

日志系统是典型的**生产者-消费者**模型：

-   **生产者**: 任何调用 `LOG_...()(...)` 接口的业务线程。
-   **消费者**: `Logger` 内部的一个专用后台工作线程 (`worker_thread_`)。
-   **缓冲区**: 一个线程安全的 `std::queue<LogRecord>`。

### 2.1. `Logger`

`Logger` 是日志系统的核心与门面。它是一个单例（通过 `Logger::getInstance()` 访问），负责管理：

-   **日志级别 (`LogLevel`)**: 控制哪些级别的日志应该被处理。
-   **日志接收器 (`sinks_`)**: 一个 `LogSink` 的 `vector`，存储所有日志的输出目的地。
-   **日志队列 (`queue_`)**: 一个线程安全的队列，用作生产者和消费者之间的缓冲区。
-   **后台工作线程 (`worker_thread_`)**: `Logger` 在构造时会启动一个 `std::jthread`。这个线程是唯一的消费者，它的工作循环 (`worker_loop`) 就是不断地从队列中取出 `LogRecord`，并分发给所有 `sinks_`。

### 2.2. `LogRecord`

这是一个简单的数据类，封装了单条日志的所有信息，包括：日志级别、时间戳、格式化后的消息、源文件名和行号。

### 2.3. `LogSink`

这是一个抽象基类（接口），定义了所有日志输出目的地必须实现的功能：

-   `virtual bool log(const LogRecord& record) = 0;`：将一条日志记录写入到具体目的地。
-   `virtual bool flush() = 0;`：将缓冲区的内容强制刷写到目的地。

已实现的具体 Sinks 包括：

-   **`ConsoleSink`**: 将日志打印到标准输出/标准错误。
-   **`FileSink`**: 将日志写入到指定的文件。

### 2.4. `LOG_...` 调用接口

为了提供最方便的调用方式，我们提供了一系列函数，如 `LOG_INFO`, `LOG_ERROR` 等，它们返回一个临时的可调用对象。当你写下 `LOG_INFO()("Hello {}", "World");` 时：

1.  `LOG_INFO()` 函数被调用，它创建一个临时的 `LogInfoImpl` 对象。这个对象的构造函数会捕获当前的源码位置 (`std::source_location`)。
2.  你紧接着用第二对 `()` 并传入格式化参数来调用这个临时对象，这会触发它的 `operator()`。
3.  `operator()` 首先检查日志级别，如果当前级别允许，它会使用 `std::format` 格式化字符串，然后创建一个 `LogRecord` 对象。
4.  最后，它将这个 `LogRecord` 推入 `Logger` 的全局队列中，并唤醒后台工作线程。

## 3. 如何使用

```cpp
import kvdb.logging;
import std;

int main() {
    // 1. 获取 Logger 单例
    auto& logger = kvdb::logging::Logger::getInstance();

    // 2. (可选) 设置全局日志级别，默认为 INFO
    logger.setLevel(kvdb::logging::LogLevel::DEBUG);

    // 3. (可选) 添加一个新的 Sink，比如文件输出
    auto file_sink = kvdb::logging::FileSink::create("my_app.log");
    if (file_sink) {
        logger.addSink(*file_sink);
    }

    // 4. 使用日志接口
    LOG_DEBUG()("这是一个调试信息，通常不会显示。");
    LOG_INFO()("程序启动成功。");
    
    std::string user = "Alice";
    int user_id = 123;
    LOG_WARNING()("用户 '{}' (ID: {}) 尝试了无效操作。", user, user_id);

    try {
        throw std::runtime_error("发生了一个示例错误");
    } catch (const std::exception& e) {
        LOG_ERROR()("捕获到异常: {}", e.what());
    }

    // Logger 会在程序结束时自动析构，确保所有日志都被刷写
    return 0;
}
```