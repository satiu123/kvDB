# kvDB LSM-Tree 设计文档

本文档记录了 `kvDB` 从一个简单的哈希表数据库演进到基于日志结构合并树 (LSM-Tree) 的存储引擎后的核心设计和实现细节。

---

## 1. 当前架构

`kvDB` 目前实现了一个简化的、功能完备的 LSM-Tree 存储引擎，其核心组件如下：

### 1.1. 核心组件

-   **预写式日志 (WAL - `storage::Wal`)**
    -   所有的数据写入和删除操作在应用到内存之前，都会先以记录的形式追加到 WAL 文件中。
    -   这确保了操作的持久性。即使服务器在数据刷写到磁盘前崩溃，也可以通过重放 WAL 来恢复内存状态。

-   **内存表 (MemTable - `Database::data_`)**
    -   这是一个在内存中的、有序的 `std::map`。
    -   所有新的写入请求（`put`, `remove`）都会首先进入 MemTable。

-   **不可变内存表 (Immutable MemTable - `Database::immutable_memtable_`)**
    -   当 MemTable 的大小达到预设阈值 (`memtable_flush_threshold_`) 时，它会被“冻结”为一个不可变 MemTable。
    -   此时，一个新的空 MemTable 会被创建，用于接收后续的写入请求。
    -   不可变 MemTable 在后台等待被刷写到磁盘。

-   **排序字符串表 (SSTable - `storage::SSTable`)**
    -   磁盘上不可变的、有序的文件。
    -   不可变 MemTable 的内容会被一个刷写 (Flush) 操作写入一个新的 SSTable 文件中。
    -   **文件格式**: 我们实现了一个二进制格式，包含数据块 (Data Blocks)、索引块 (Index Block) 和文件尾注 (Footer)，支持高效的查找。

### 1.2. 核心流程

-   **写入路径 (`put`)**
    1.  操作追加到 WAL。
    2.  数据写入可变的 MemTable。
    3.  如果 MemTable 已满，则将其转换为不可变 MemTable，并异步刷写到一个新的 SSTable 文件中。

-   **删除路径 (`remove`)**
    1.  删除操作被实现为写入一条**墓碑 (Tombstone)** 记录。
    2.  我们使用一个**空字符串**作为墓碑标记。
    3.  这个墓碑记录会像普通数据一样，流经 WAL、MemTable 和 SSTable。

-   **读取路径 (`get`)**
    1.  **顺序查找**: 可变 MemTable -> 不可变 MemTable -> SSTable 文件（从新到旧）。
    2.  一旦找到一个键，查找立即停止并返回结果。
    3.  如果找到的是一条墓碑记录，则返回“未找到”。

-   **合并 (`compact`)**
    -   我们实现了一个**手动触发**的 `compact()` 方法。
    -   它会读取所有现存的 SSTable 文件，在内存中进行多路归并排序，并写入一个全新的、合并后的 SSTable 文件。
    -   在合并过程中，被覆盖的旧数据和被墓碑标记的数据会被彻底清除，从而回收磁盘空间并优化未来的读取性能。

---
