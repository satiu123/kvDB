# kvDB LSM-Tree 设计文档

本文档记录了 `kvDB` 从一个简单的哈希表数据库演进到基于日志结构合并树 (LSM-Tree) 的存储引擎后的核心设计，以及未来的优化方向。

---

## 1. 当前架构 (Current Architecture)

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

## 2. 待办事项 (TODO) / 未来方向

### 2.1. 优先级 - 高 (High Priority)

-   **代码重构：移除旧快照系统**
    -   当前的 LSM-Tree 架构已经提供了完整的持久化和恢复机制，原有的 `Snapshot` 和 `RecoveryManager` 已经冗余。
    -   **计划**: 删除所有与 `Snapshot` 相关的类和测试，简化 `Database` 的启动恢复逻辑，使其完全依赖于 SSTable 和 WAL。

-   **并发改进：后台任务**
    -   当前的刷写 (Flush) 和合并 (Compaction) 操作是在前台同步执行的，这会阻塞新的写入请求。
    -   **计划**: 将 Flush 和 Compaction 移到专用的后台线程中执行。

### 2.2. 优先级 - 中 (Medium Priority)

-   **SSTable 优化：布隆过滤器 (Bloom Filter)**
    -   为了避免在不存在的键上进行不必要的磁盘读取，可以为每个 SSTable 增加一个布隆过滤器。
    -   在查询一个键之前，先检查布隆过滤器。如果它说“肯定不存在”，则直接跳过该文件。

-   **恢复流程优化**
    -   当前的恢复逻辑需要重放整个 WAL 文件，效率较低。
    -   **计划**: 在刷写 SSTable 时，记录下当时 WAL 的序列号或偏移量。在恢复时，只需重放该位置之后的 WAL 记录即可。

### 2.3. 优先级 - 低 (Low Priority)

-   **SSTable 优化：前缀压缩 (Prefix Compression)**
    -   由于 SSTable 中的键是有序的，相邻的键通常有共同的前缀。可以使用前缀压缩来减小索引块和数据块的体积。

-   **分层合并策略 (Leveled Compaction)**
    -   实现一个更成熟的自动合并策略，例如 LevelDB/RocksDB 中使用的分层合并。
    -   将 SSTable 组织成多个层级 (L0, L1, ...)，当某一层的文件数量或大小达到阈值时，自动触发向下一层的合并。

-   **缓存 (Cache)**
    -   为 SSTable 的数据块增加一个 LRU 缓存，以加速对热点数据的访问。
