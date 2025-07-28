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

-   **核心操作**: 提供 `put`, `get`, `remove`, `exists` 等基础键值操作。
-   **持久化存储**:
    -   **✍️ 预写式日志 (WAL)**: 确保所有操作在写入内存前都已记录，保证数据安全。
    -   **📸 快照**: 支持创建数据快照，用于快速恢复和启动。
-   **并发安全**: 通过互斥锁为数据库操作提供基本的线程安全保障。
-   **📦 模块化设计**: 全面采用 C++20 模块，实现清晰、现代的架构。
-   **🔧 配套工具**:
    -   一个灵活的日志系统，支持多路输出。
    -   一个简单的命令行界面 (CLI)，方便直接与数据库交互。

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

这是一个如何将 `kvDB` 作为库使用的简单示例：

```cpp
import kvdb;
import std;

int main() {
    // 创建一个数据库实例
    kvdb::core::Database db("my_database.wal");

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

项目还构建了一个简单的命令行工具，方便进行快速测试和数据管理。

```bash
# 运行 CLI
./build/kvdb-cli

# 在 CLI 内部
kvdb> set mykey myvalue
OK
kvdb> get mykey
myvalue
kvdb> exit
再见!
```

## 📄 许可证

该项目根据 [MIT 许可证](LICENSE) 授权。