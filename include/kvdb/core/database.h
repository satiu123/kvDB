#ifndef KVDB_DATABASE_H
#define KVDB_DATABASE_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "kvdb/storage/recovery_manager.h"
#include "kvdb/storage/snapshot.h"
#include "kvdb/storage/snapshot_manager.h"
#include "kvdb/storage/wal/wal.h"

namespace kvdb {

class Database {
  public:
    ~Database();
    Database(std::string_view wal_path = "kvdb.wal",
             std::string_view snapshot_path = "kvdb.snapshot");
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

    // 快照相关操作

    /**
     * @brief 创建数据快照
     * @return 是否成功
     */
    bool createSnapshot();

    /**
     * @brief 设置快照配置
     * @param config 快照配置
     */
    void setSnapshotConfig(const SnapshotConfig& config);

    /**
     * @brief 获取当前快照配置
     * @return 快照配置
     */
    const SnapshotConfig& getSnapshotConfig() const;

    /**
     * @brief 检查是否存在快照文件
     * @return 是否存在
     */
    bool hasSnapshot() const;

  private:
    std::unordered_map<std::string, std::string> data_;
    mutable std::mutex mutex_;

    Wal wal_;                           // WAL实例，用于持久化操作
    Snapshot snapshot_;                 // 快照实例，用于快照功能
    RecoveryManager recovery_manager_;  // 恢复管理器
    SnapshotManager snapshot_manager_;  // 快照管理器

    enum class OpType : uint8_t { PUT, REMOVE, CLEAR };
};

}  // namespace kvdb

#endif  // KVDB_DATABASE_H
