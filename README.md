# kvDB

一个高性能、线程安全的键值数据库，采用现代 C++20 编写，具有预写式日志（WAL）支持。

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![CMake](https://img.shields.io/badge/CMake-3.12+-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 特性

- ⚡ **高性能**: 基于内存的哈希表存储，快速的键值操作
- 🔒 **线程安全**: 使用互斥锁保护并发访问
- 💾 **持久化**: 预写式日志（WAL）确保数据持久性和一致性
- 🔧 **现代 C++**: 使用 C++20 特性，如 std::string_view 和模板
- 📝 **完整日志系统**: 支持多种日志输出方式（控制台、文件）
- 🧪 **全面测试**: 包含完整的单元测试套件
- 📚 **简单易用**: 清晰的 API 设计，易于集成

## 快速开始

### 依赖要求

- C++20 兼容的编译器（GCC 10+, Clang 10+, MSVC 2019+）
- CMake 3.12 或更高版本
- Google Test（用于运行测试，可选）

### 编译

```bash
# 克隆仓库
git clone https://github.com/satiu123/kvDB.git
cd kvDB

# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake ..
make -j$(nproc)

# 运行测试（可选）
ctest --verbose

# 运行示例
./examples/simple_example
```

### 基本使用

```cpp
#include "kvdb/database.h"
#include <iostream>

int main() {
    // 创建数据库实例，指定 WAL 文件路径
    kvdb::Database db("my_database.wal");
    
    // 插入键值对
    db.put("name", "kvDB");
    db.put("version", "0.1.0");
    db.put("language", "C++");
    
    // 获取值
    auto name = db.get("name");
    if (name) {
        std::cout << "Name: " << *name << std::endl;
    }
    
    // 检查键是否存在
    if (db.exists("version")) {
        std::cout << "Version exists!" << std::endl;
    }
    
    // 删除键
    db.remove("language");
    
    // 获取数据库大小
    std::cout << "Database size: " << db.size() << " entries" << std::endl;
    
    // 清空数据库
    db.clear();
    
    return 0;
}
```

## API 文档

### Database 类

`kvdb::Database` 是主要的数据库接口类。

#### 构造函数

```cpp
Database(std::string_view wal_path = "kvdb.wal");
```

创建一个新的数据库实例。

- `wal_path`: WAL 文件的路径，默认为 "kvdb.wal"

#### 基本操作

```cpp
// 插入或更新键值对
bool put(std::string_view key, std::string_view value);

// 获取键对应的值
std::optional<std::string> get(std::string_view key) const;

// 删除指定键
bool remove(std::string_view key);

// 检查键是否存在
bool exists(std::string_view key) const;

// 获取数据库中键值对的数量
size_t size() const;

// 清空数据库
void clear();
```

### WAL（预写式日志）

kvDB 使用预写式日志来确保数据的持久性和一致性。所有的写操作在修改内存数据之前都会先记录到 WAL 文件中。

#### 操作类型

- `PUT`: 插入或更新键值对
- `REMOVE`: 删除键
- `CLEAR`: 清空数据库

#### WAL 恢复

当数据库重新启动时，它会自动从 WAL 文件中恢复数据：

```cpp
// 数据库会在构造时自动恢复
kvdb::Database db("my_database.wal");  // 自动从 WAL 恢复数据
```

### 日志系统

kvDB 包含一个完整的日志系统，支持不同的输出目标。

```cpp
#include "kvdb/logger.h"
#include "kvdb/sinks/console_sink.h"
#include "kvdb/sinks/file_sink.h"

// 配置日志系统
auto& logger = kvdb::Logger::getInstance();
logger.addSink(std::make_shared<kvdb::ConsoleSink>());
logger.addSink(std::make_shared<kvdb::FileSink>("app.log"));
logger.setLevel(kvdb::LogLevel::INFO);
```

#### 日志级别

- `DEBUG`: 调试信息
- `INFO`: 一般信息
- `WARNING`: 警告信息
- `ERROR`: 错误信息

## 架构设计

### 组件概览

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│    Database     │───▶│      WAL        │───▶│   WalRecord     │
│   (主接口)       │    │  (预写式日志)    │    │   (日志记录)     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │
         ▼
┌─────────────────┐    ┌─────────────────┐
│     Logger      │───▶│    LogSink      │
│   (日志系统)     │    │  (日志输出)      │
└─────────────────┘    └─────────────────┘
```

### 数据流

1. **写操作流程**:
   - 用户调用 `put()` 或 `remove()`
   - 操作首先记录到 WAL 文件
   - WAL 记录同步到磁盘
   - 更新内存中的数据结构

2. **读操作流程**:
   - 用户调用 `get()` 或 `exists()`
   - 直接从内存中的哈希表读取
   - 返回结果给用户

3. **恢复流程**:
   - 数据库启动时读取 WAL 文件
   - 重放所有的 WAL 记录
   - 重建内存中的数据结构

## 性能特性

- **内存操作**: 所有读操作都在内存中进行，提供O(1)的平均时间复杂度
- **WAL 优化**: 批量写入和同步操作减少磁盘 I/O
- **线程安全**: 细粒度锁定，支持高并发访问
- **零拷贝**: 使用 `std::string_view` 避免不必要的字符串复制

## 贡献指南

欢迎贡献代码！请遵循以下步骤：

1. Fork 这个仓库
2. 创建您的特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交您的更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开一个 Pull Request

### 代码规范

- 使用现代 C++ 特性（C++20）
- 遵循 Google C++ 风格指南
- 添加充分的单元测试
- 为公共 API 添加详细的文档注释

## 测试

运行所有测试：

```bash
cd build
ctest --verbose
```

运行特定测试：

```bash
# 测试数据库功能
./tests/kvdb_tests --gtest_filter="DatabaseTest.*"

# 测试 WAL 功能
./tests/kvdb_tests --gtest_filter="WalTest.*"

# 测试日志系统
./tests/kvdb_tests --gtest_filter="LoggerTest.*"
```

## 许可证

本项目使用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 作者

- **satiu123** - *初始工作* - [satiu123](https://github.com/satiu123)

## 致谢

- 感谢所有贡献者
- 灵感来源于现代数据库系统的设计原则
- 使用了 Google Test 进行单元测试
