# kvDB

<p align="center">
  <img src="https://img.shields.io/badge/C++-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License: MIT">
  <img src="https://img.shields.io/badge/Status-In%20Development-orange.svg" alt="Status: In Development">
</p>

一个使用现代 C++ (C++23) 编写的简单键值数据库，专注于学习和实验。

> ### ⚠️ 项目状态：开发中
>
> 该项目目前正在积极开发中，其 API 和底层文件格式未来可能会发生变化。

---

## 📜 目录

- [kvDB](#kvdb)
  - [📜 目录](#-目录)
  - [✨ 特性](#-特性)
  - [🛠️ 构建指南](#️-构建指南)
  - [💡 使用示例](#-使用示例)
    - [作为库使用](#作为库使用)
    - [命令行工具 (CLI)](#命令行工具-cli)
  - [📄 许可证](#-许可证)

---

## ✨ 特性

-   **核心操作**: 提供 `put`, `get`, `remove`, `exists` 等基础键值操作，并支持 `clear`, `compact`, `keys`, `size` 等数据库管理命令。
-   **LSM-Tree 存储引擎**（最新架构）:
    -   **预写式日志 (WAL)**：所有写操作先追加到 WAL 文件，保证持久性。
    -   **可变/不可变 MemTable**：写入操作先进入内存表，满后冻结并异步刷入 SSTable。
    -   **SSTable**：磁盘有序不可变文件，支持高效查找和合并（手动触发 `compact`）。
    -   **合并与清理**：支持手动 compaction，合并多份 SSTable，清理已删除（tombstone）和被覆盖的数据。
    -   **命令行体验**：CLI 命令全面升级，支持 put/get/remove/exists/size/keys/clear/compact/wal/exit/help。
-   **并发安全**: 通过互斥锁为数据库操作提供基本的线程安全保障。
-   **模块化设计**: 全面采用 C++20 模块，实现清晰、现代的架构。
-   **日志系统**: 灵活的多级日志，支持多路输出。
-   **可扩展性**: 设计文档与实现同步更新，便于后续增加 Bloom Filter、Cache、分层合并等功能。
详细设计和优化方向见 [LSM_DESIGN.md](docs/LSM_DESIGN.md)、[SSTABLE_FORMAT.md](docs/SSTABLE_FORMAT.md)。
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

### 作为库使用

```cpp
import kvdb;
import std;

int main() {
    // 创建一个数据库实例（新版本需指定目录）
    kvdb::core::Database db("my_db_dir");

    // 存储一些键值对
    db.put("project", "kvDB");
    db.put("language", "C++23");

    // 检索一个值
    if (auto value = db.get("project")) {
        std::cout << "项目: " << *value << std::endl;
    }

    // 查看数据库大小
    std::cout << "数据库大小: " << db.size() << std::endl;

    return 0;
}
```

### 命令行工具 (CLI)

项目还构建了一个简单的命令行工具，方便进行快速测试和数据管理。命令支持：

- `put <key> <value>`：插入或更新键值对
- `get <key>`：查询键
- `remove <key>`：删除键
- `exists <key>`：判断键是否存在
- `size`：数据库条目数量
- `keys`：列出所有键
- `clear`：清空数据库
- `compact`：手动合并 SSTable
- `wal`：打印 WAL 记录
- `help`：显示帮助
- `exit/quit`：退出

```bash
# 运行 CLI
./build/kvdb-cli

# 在 CLI 内部
kvdb> put mykey myvalue
OK
kvdb> get mykey
myvalue
kvdb> keys
mykey
kvdb> compact
OK
kvdb> exit
Goodbye!
```

## 📄 许可证

该项目根据 [MIT 许可证](LICENSE) 授权。


