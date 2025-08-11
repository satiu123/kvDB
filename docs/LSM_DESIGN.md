# kvDB LSM-Tree 设计文档 (异步架构版)

本文档记录了 `kvDB` 在演进到基于**异步 I/O** 和 **C++20 协程**的日志结构合并树 (LSM-Tree) 存储引擎后的核心设计。

---

## 1. 异步架构概述

`kvDB` 实现了一个完全异步的 LSM-Tree 存储引擎，旨在最大化 I/O 吞吐量和并发性能。其核心驱动力是 `io_uring`，一个高性能的 Linux 异步 I/O 接口，以及 C++20 协程 (`kvdb::coro::Task`)，它使得编写复杂的异步逻辑如同编写同步代码一样简单。

与传统的多线程加锁模型不同，本设计中几乎所有的 I/O 操作（包括文件读写）都是非阻塞的。

### 1.1. 核心组件

-   **IOUring (`kvdb::core::io::IOUring`)**
    -   整个数据库的异步事件循环和 I/O 调度器。
    -   `AsyncDatabase` 实例拥有一个 `IOUring`，并将其引用传递给所有需要执行异步 I/O 的子组件（如 WAL, SSTable）。

-   **协程任务 (`kvdb::core::coro::Task`)**
    -   所有对外暴露的数据库操作（如 `async_put`, `async_get`）都返回一个 `Task`。
    -   这代表一个可以被挂起和恢复的异步工作单元。当一个任务需要等待 I/O 操作时，它会 `co_await` 一个 `Awaiter`，将自己挂起，并允许 `IOUring` 执行其他任务，直到 I/O 完成。

-   **异步预写式日志 (WAL - `kvdb::storage::AsyncWal`)**
    -   所有写入操作在应用到内存表之前，都会通过 `io_uring` **异步地**提交到 WAL 文件。
    -   这确保了操作的持久性，同时不会阻塞工作线程。

-   **内存表 (MemTable - `AsyncDatabase::memtable_`)**
    -   一个在内存中的、有序的 `std::map`。
    -   所有新的写入请求在 WAL 记录成功提交后，会进入此 MemTable。

-   **不可变内存表 (Immutable MemTable - `AsyncDatabase::immutable_memtable_`)**
    -   当 MemTable 的大小达到阈值 (`flush_threshold_`) 时，它会被移动到一个不可变 MemTable 指针中。
    -   同时，一个新的空 MemTable 会被创建，用于接收后续写入。
    -   一个后台的**异步刷写任务** (`flush_memtable_to_sstable`) 会被触发，将不可变 MemTable 的内容写入新的 SSTable 文件。

-   **异步排序字符串表 (SSTable - `kvdb::storage::SSTable`)**
    -   磁盘上不可变的、有序的文件。所有对 SSTable 的读写操作都是通过 `io_uring` 实现的异步 `Task`。
    -   其文件格式保持不变（数据块、索引块、布隆过滤器、尾注），但构建和查找过程已完全异步化。

### 1.2. 核心异步流程

-   **写入路径 (`async_put`)**
    1.  调用 `async_put` 返回一个 `Task`。
    2.  任务执行时，首先 `co_await wal_->async_append_put(...)`，将记录异步写入 WAL。
    3.  WAL 写入完成后，任务恢复，并将数据写入可变的 MemTable。
    4.  检查 MemTable 大小。如果超过阈值，则启动一个**无需等待**的后台任务 `flush_memtable_to_sstable()`，该任务会异步地将当前 MemTable 内容刷写到新的 SSTable。

-   **删除路径 (`async_remove`)**
    -   与写入路径类似，但写入的是一条**墓碑 (Tombstone)** 记录（值为一个特殊空标记）。

-   **读取路径 (`async_get`)**
    1.  调用 `async_get` 返回一个 `Task`。
    2.  任务执行时，按顺序查找：
        a.  在可变 MemTable 中查找。如果找到，直接返回结果。
        b.  在不可变 MemTable 中查找。如果找到，直接返回结果。
        c.  如果内存中未找到，则从新到旧遍历所有 SSTable。对于每个 SSTable，`co_await sstable->find(key)`。
    3.  `sstable->find` 任务内部会首先检查布隆过滤器，然后异步读取索引块和数据块来查找键。
    4.  一旦找到键，立即返回结果。如果找到的是墓碑，则返回“未找到”。

-   **合并 (`compact`)**
    -   合并过程同样被设计为一个异步任务。
    -   它会异步地读取所有 SSTable，在内存中进行归并排序，然后异步地将结果写入一个新的 SSTable 文件，最后更新 Manifest。整个过程不会阻塞新的读写请求。