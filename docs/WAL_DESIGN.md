# kvDB 预写日志 (WAL) 设计文档 (异步架构版)

## 1. 概述 (Overview)

预写日志 (Write-Ahead Log, WAL) 是 `kvDB` 实现持久性和原子性的核心组件。在新版异步架构中，WAL 的功能通过 `kvdb::storage::AsyncWal` 模块实现，它利用 `io_uring` 提供了一个完全非阻塞的日志系统。

其基本原则依然是在任何数据变更应用到内存中的 MemTable 之前，必须先将描述该操作的日志记录**异步地**写入到磁盘上的 WAL 文件中。这确保了即使系统崩溃，也能通过重放 WAL 文件来恢复数据，同时不会在写入时阻塞关键任务。

- **主要模块**: `kvdb::storage::AsyncWal`, `kvdb::storage::WalRecord`
- **磁盘文件**: `data/wal/kvdb.wal`

## 2. 核心组件 (Core Components)

### 2.1. `WalRecord`

`WalRecord` 是 WAL 中最基本的原子单元，它封装了一次数据库操作。其定义和磁盘上的二进制布局保持不变，以确保向后兼容和数据完整性。

- **主要属性**:
  - `op_type_`: 操作类型 (`PUT`, `REMOVE`, `CLEAR`)。
  - `key_`, `value_`: 操作的键和值。
  - `sequence_number_`: 单调递增的逻辑时间戳。
  - `checksum_`: 用于验证记录完整性的 CRC32 校验和。

### 2.2. `AsyncWal`

`AsyncWal` 类是 WAL 文件的异步管理器。它与 `IOUring` 紧密集成，负责：

- **异步文件I/O**: 通过 `io_uring` 打开、关闭、读取和写入 `kvdb.wal` 文件，所有操作都以 `kvdb::coro::Task` 的形式进行。
- **异步记录追加**: 将 `WalRecord` 对象序列化后，通过 `io_uring` 的写请求队列追加到文件末尾。
- **异步日志重放**: 在数据库启动时，提供一个 `async_replay` 方法，它返回一个 `Task`，允许调用者异步地、逐条地读取 WAL 文件并应用恢复逻辑。
- **并发安全**: 由于所有 I/O 操作都通过同一个 `IOUring` 实例调度，避免了传统多线程模型中的数据竞争，因此无需使用 `std::mutex`。序列号的分配则通过 `std::atomic` 保证原子性。

## 3. 磁盘格式 (On-Disk Format)

WAL 文件由一系列连续的 `WalRecord` 记录组成。该格式与旧版完全兼容，以确保数据可迁移性。

**单条记录的精确布局如下:**

```
+------------------------+--------------------------+------------------------------------+
| 总长度 (Total Length)  | CRC32 校验和 (Checksum)  | 数据负载 (Payload)                 |
| (4 字节, uint32_t)     | (4 字节, uint32_t)       | (变长)                             |
+------------------------+--------------------------+------------------------------------+
```

- **总长度**: 记录了这条记录的总字节数。
- **CRC32 校验和**: 对 **数据负载 (Payload)** 部分计算出的校验和。
- **数据负载**: 包含了操作的实际信息（操作类型、Key、Value、序列号）。

## 4. 异步工作流程 (Asynchronous Workflow)

### 4.1. 记录追加 (Append)

当执行一次写操作时（如 `db->async_put()`），流程如下：

1.  `AsyncDatabase` 调用 `async_wal_->async_append_put()`，该方法返回一个 `Task`。
2.  `AsyncWal` 创建一个 `WalRecord` 对象，并为其分配一个新的原子递增的 `sequence_number`。
3.  `WalRecord` 被序列化为二进制格式。
4.  `AsyncWal` 向 `IOUring` 提交一个写请求，将序列化后的数据写入 WAL 文件。
5.  调用者 `co_await` 这个追加任务。当前协程被挂起，直到 `IOUring` 通知写操作完成。这期间，CPU 可以执行其他不相关的任务。

### 4.2. 数据库恢复 (Recovery)

当数据库启动时，`AsyncDatabase::init()` 方法会执行异步的恢复流程：

1.  **加载 Manifest**: `AsyncDatabase` 首先异步加载 `Manifest` 文件，获取 `last_wal_sequence_number` 作为恢复的检查点。
2.  **请求重放**: `AsyncDatabase` 调用 `wal_->async_replay()` 并 `co_await` 其返回的 `Task`。`async_replay` 接受一个回调函数（Lambda）作为参数。
3.  **过滤并重放**: `async_replay` 任务内部会异步地、逐条地从 `kvdb.wal` 文件中读取记录。
    - 对于每一条成功读取并校验通过的记录，`AsyncDatabase` 提供的回调函数会检查其序列号。
    - 如果记录的序列号**大于** `last_wal_sequence_number`，则说明该操作未被持久化到 SSTable，需要将其应用到内存中的 MemTable。
    - 这个检查避免了重复加载数据，加快了启动速度。
4.  **完成恢复**: 当 `async_replay` 任务完成时，MemTable 就恢复到了系统崩溃前的状态。`AsyncWal` 内部的序列号计数器也会更新，以保证后续操作的连续性。