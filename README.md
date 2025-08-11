# kvDB

<p align="center">
  <img src="https://img.shields.io/badge/C++-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License: MIT">
  <img src="https://img.shields.io/badge/Status-In%20Development-orange.svg" alt="Status: In Development">
</p>

一个基于 **C++23** 编写的高性能、全异步键值数据库，其核心是基于 **C++20 协程** 和 **io_uring** 的日志结构合并树 (LSM-Tree) 存储引擎。

> ### ⚠️ 项目状态：开发中
>
> 该项目目前正在积极开发中，其 API 和底层文件格式未来可能会发生变化。

---

## 📜 目录
- [kvDB](#kvdb)
  - [📜 目录](#-目录)
  - [✨ 核心技术与特性](#-核心技术与特性)
  - [🏗️ 异步架构](#️-异步架构)
  - [🛠️ 构建指南](#️-构建指南)
  - [💡 使用示例](#-使用示例)
    - [作为库使用 (异步 API)](#作为库使用-异步-api)
    - [命令行工具 (CLI)](#命令行工具-cli)
  - [📚 设计文档](#-设计文档)
  - [📄 许可证](#-许可证)

---

## ✨ 核心技术与特性

-   **C++23 标准**: 全面拥抱最新 C++ 标准，使用模块、协程、`std::expected` 等现代化特性。
-   **高性能异步核心**:
    -   **io_uring**: 使用 Linux 最新的异步 I/O 接口，实现极致的 I/O 吞吐量。
    -   **C++20 协程**: 将复杂的异步回调逻辑转化为简洁的顺序代码，所有数据库操作均为非阻塞。
-   **模块化设计**: 完全用 C++20 模块 (`.cppm`) 组织代码，告别传统头文件，实现清晰、快速的构建。
-   **LSM-Tree 存储引擎**:
    -   **异步预写式日志 (AsyncWAL)**：所有写操作首先异步提交到 WAL，确保持久性且不阻塞。
    -   **内存表 (MemTable)**：数据首先写入内存中的有序表，达到阈值后冻结。
    -   **异步刷写 (Flush)**：不可变的 MemTable 会在后台异步地刷写为磁盘上的 SSTable 文件。
    -   **排序字符串表 (SSTable)**：磁盘上不可变的有序文件，支持高效的异步查找和合并。

## 🏗️ 异步架构

本项目的核心是 `AsyncDatabase`，它管理着一个 `io_uring` 实例作为事件循环。所有操作如 `async_put`, `async_get` 都返回一个 `kvdb::coro::Task`。当任务需要 I/O 时，它会 `co_await` 一个 `Awaiter` 对象，将自身挂起并向 `io_uring` 提交请求。当 I/O 完成后，`io_uring` 会通过协程句柄恢复对应的任务。

这种设计避免了传统多线程模型的锁竞争和上下文切换开销，实现了高效的单线程事件驱动模型。

## 🛠️ 构建指南

您需要一个支持 C++23 和 C++20 模块的现代 C++ 编译器（例如，Clang 17+ 或 GCC 13+）。

```bash
# 1. 克隆仓库
git clone https://github.com/satiu/kvDB.git
cd kvDB

# 2. 使用 CMake 配置项目 (推荐使用 Ninja)
cmake -B build -G Ninja

# 3. 构建项目
cmake --build build
```

## 💡 使用示例

### 作为库使用 (异步 API)

由于所有 API 都是异步的，您需要将操作包装在一个 `Task` 中，并通过 `db.run()` 来执行它。

```cpp
import kvdb;
import std;

// 定义一个执行数据库操作的异步任务
kvdb::core::coro::Task<void> my_database_task(kvdb::core::AsyncDatabase& db) {
    // 异步存储键值对
    co_await db.async_put("project", "kvDB");
    co_await db.async_put("language", "C++23");
    co_await db.async_put("feature", "Coroutine");

    // 异步检索一个值
    if (auto value = co_await db.async_get("project")) {
        std::cout << "项目: " << *value << std::endl;
    }

    // 异步删除一个值
    co_await db.async_remove("feature");
}

int main() {
    // 创建一个数据库实例
    kvdb::core::AsyncDatabase db("my_db_dir");
    
    // 初始化数据库（会异步加载 manifest 和 wal）
    db.run(db.init());

    // 运行我们定义的异步任务
    db.run(my_database_task(db));

    return 0;
}
```

### 命令行工具 (CLI)

项目还构建了一个简单的命令行工具，方便进行快速测试和数据管理。

-   **支持命令**: `put`, `get`, `remove`, `exists`, `size`, `keys`, `clear`, `compact`, `wal`, `help`, `exit`

```bash
# 运行 CLI
./build/kvdb-cli [数据库目录]

# 在 CLI 内部
kvdb> put mykey myvalue
OK
kvdb> get mykey
myvalue
kvdb> exit
Goodbye!
```

## 📚 设计文档

想了解更多关于 `kvDB` 内部实现的细节吗？请查阅 `docs/` 目录下的设计文档：

-   [LSM-Tree 异步架构设计](docs/LSM_DESIGN.md)
-   [协程框架设计](docs/COROUTINE_DESIGN.md)
-   [io_uring 封装设计](docs/IO_URING_DESIGN.md)
-   [SSTable 文件格式](docs/SSTABLE_FORMAT.md)
-   [WAL (预写日志) 设计](docs/WAL_DESIGN.md)
-   [线程池设计](docs/THREAD_POOL_DESIGN.md)
-   [日志系统设计](docs/LOGGING_DESIGN.md)
-   [二进制工具与缓冲区设计](docs/BINARY_UTILS_DESIGN.md)
## 📄 许可证

该项目根据 [MIT 许可证](LICENSE) 授权。