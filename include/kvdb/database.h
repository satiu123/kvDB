#ifndef KVDB_DATABASE_H
#define KVDB_DATABASE_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "kvdb/wal/wal.h"

namespace kvdb {

class Database {
  public:
    Database() = delete;  // 禁用默认构造函数
    ~Database();
    Database(std::string_view wal_path = "kvdb.wal");
    // 禁用拷贝和移动构造函数
    // 禁用拷贝和移动赋值运算符
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    // 基本操作

    /*
     * @brief 插入或更新键值对
     * @param key 键
     * @param value 值
     * @return 是否成功
     */

    bool put(std::string_view key, std::string_view value);

    /*
     * @brief 获取键对应的值
     * @param key 键
     * @return 值，如果不存在则返回std::nullopt
     */

    std::optional<std::string> get(std::string_view key) const;

    /*
     * @brief 删除指定键
     * @param key 键
     * @return 是否成功
     */

    bool remove(std::string_view key);

    // 高级操作
    size_t size() const;
    void clear();

    /*
     * @brief 检查键是否存在
     * @param key 键
     * @return 是否存在
     */

    bool exists(std::string_view key) const;

    bool replayWAL(const std::function<bool(const WalRecord&)>& handler) {
        return wal_.replay(handler);
    }

  private:
    std::unordered_map<std::string, std::string> data_;
    mutable std::mutex mutex_;

    Wal wal_;  // WAL实例，用于持久化操作
    enum class OpType : uint8_t { PUT, REMOVE, CLEAR };

    // 从WAL文件恢复数据
    void recoverFromWal();
};

}  // namespace kvdb

#endif  // KVDB_DATABASE_H
