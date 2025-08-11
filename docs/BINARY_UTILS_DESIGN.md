# kvDB 二进制工具与缓冲区设计文档

## 1. 设计目标

`kvdb.core.binary` 模块提供了一套底层工具，用于在内存中高效、安全地处理二进制数据。其设计目标是：

-   **高效序列化/反序列化**：为数据库中各种数据结构（如 WAL 记录、SSTable 索引）提供快速转换为字节流或从字节流解析的能力。
-   **内存安全**：通过 `std::span` 和清晰的所有权模型，防止缓冲区溢出和内存访问错误。
-   **零拷贝读取**：在解析数据时，尽可能地使用 `std::string_view` 和 `std::span` 来引用底层缓冲区的数据，而不是进行昂贵的数据拷贝。
-   **提供两种缓冲区模型**：
    1.  **拥有所有权的缓冲区 (`BytesBuffer`)**：用于从零开始构建一块二进制数据。
    2.  **非拥有所有权的视图 (`BytesBufferView`)**：用于在已分配的内存上进行读写，是实现零拷贝操作的核心。

## 2. 核心组件

### 2.1. `BytesBuffer`

这是一个拥有所有权的、仅供追加的缓冲区。

-   **用途**：当你需要动态构建一个字节序列时使用，例如，在将一个复杂的对象序列化以便写入文件之前。
-   **实现**: 内部持有一个 `std::vector<std::byte>`。`push` 和 `push_string` 方法用于向 `vector` 的末尾追加数据。
-   **所有权**: `BytesBuffer` 对象拥有其底层 `std::vector` 的生命周期。

### 2.2. `BytesBufferView`

这是一个非拥有所有权的缓冲区视图，是本模块中最核心和最常用的组件。

-   **用途**: 用于解析（反序列化）一块已有的内存，或在预先分配好的缓冲区上进行写入（序列化）。
-   **实现**: 内部持有一个 `std::span`。它不拥有任何内存，只是一个指向某块内存的“视图”。它维护一个内部偏移量 `offset_` 来跟踪当前的读写位置。
-   **零拷贝读取**: `read_string_view()` 方法是其设计的精髓。当从缓冲区中读取一个字符串时，它不分配新的内存来存储字符串内容，而是返回一个 `std::string_view`，该视图直接指向缓冲区中的相应位置。这在解析大量小字符串时能极大地提升性能。
-   **API**:
    -   **写入**: 提供 `write_uint32`, `write_uint64`, `write_string` 等方法，用于将数据按网络字节序（大端）或特定格式写入其管理的 `std::span`。
    -   **读取**: 提供 `read_uint32`, `read_uint64`, `read_string_view` 等方法，用于从其管理的 `std::span` 中安全地读取数据。所有读取操作都会进行边界检查，如果读取会越界，则返回 `std::unexpected`。

### 2.3. `calculate_crc32`

一个独立的辅助函数，用于计算一块字节数据的 CRC32 校验和。这在 `WAL` 和 `SSTable` 的实现中被广泛使用，以确保磁盘上数据的完整性，防止因部分写入或比特翻转导致的数据损坏。

## 3. 使用场景

### 场景一：序列化一条 WAL 记录到文件

```cpp
// 假设我们有一个 WalRecord 对象
WalRecord record = ...;

// 1. 预先分配足够大的缓冲区
std::vector<std::byte> buffer(record.size());

// 2. 创建一个写视图
BytesBufferView view(buffer);

// 3. 使用视图进行序列化
view.write_uint32(record.getTotalLength());
view.write_uint32(record.getChecksum());
// ... 写入 payload ...

// 4. 现在 buffer 中包含了完整的二进制数据，可以将其异步写入文件
co_await file.write(buffer);
```

### 场景二：从文件中读取的数据中反序列化一条记录

```cpp
// 假设我们从文件异步读取了一块数据到 read_buffer
std::vector<std::byte> read_buffer = co_await file.read(size);

// 1. 创建一个读视图，注意这里没有发生任何拷贝
BytesBufferView view(read_buffer);

// 2. 从视图中零拷贝地解析出 WalRecord
auto op_type = co_await view.read_uint8();
auto key = co_await view.read_string_view(); // -> std::string_view, no copy!
auto value = co_await view.read_string_view(); // -> std::string_view, no copy!
// ...

// 3. 用解析出的视图创建一个 WalRecord 对象
WalRecord record(op_type, key, value, ...);
```
