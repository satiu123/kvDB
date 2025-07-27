export module kvdb.storage.manager.snapshot_manager;

import std;
import kvdb.storage.wal;
import kvdb.storage.snapshot;
export namespace kvdb::storage {

/**
 * @brief 快照配置
 */
struct SnapshotConfig {
    bool auto_snapshot_enabled = false;             // 是否启用自动快照
    std::size_t wal_size_threshold = 1024 * 1024;   // WAL文件大小阈值（字节）
    std::size_t operation_count_threshold = 10000;  // 操作数量阈值
    std::chrono::minutes time_interval{60};         // 时间间隔
};

/**
 * @brief 快照管理器
 *
 * 负责管理快照的创建、配置和自动触发逻辑
 */
class SnapshotManager {
  public:
    /**
     * @brief 构造函数
     * @param wal WAL实例引用
     * @param snapshot 快照实例引用
     */
    SnapshotManager(Wal& wal, Snapshot& snapshot);

    /**
     * @brief 创建快照
     * @param data 要保存的数据
     * @return 是否成功
     */
    bool createSnapshot(const std::unordered_map<std::string, std::string>& data);

    /**
     * @brief 设置快照配置
     * @param config 快照配置
     */
    void setConfig(const SnapshotConfig& config);

    /**
     * @brief 获取快照配置
     * @return 快照配置
     */
    const SnapshotConfig& getConfig() const;

    /**
     * @brief 检查是否存在快照文件
     * @return 是否存在
     */
    bool hasSnapshot() const;

    /**
     * @brief 记录一次操作
     */
    void recordOperation();

    /**
     * @brief 检查是否需要自动创建快照
     * @param data 当前数据（用于创建快照）
     * @return 是否创建了快照
     */
    bool checkAutoSnapshot(const std::unordered_map<std::string, std::string>& data);

  private:
    Wal& wal_;
    Snapshot& snapshot_;
    SnapshotConfig config_;

    std::size_t operations_since_snapshot_ = 0;
    std::chrono::steady_clock::time_point last_snapshot_time_;

    /**
     * @brief 重置快照计数器
     */
    void resetCounters();
};

}  // namespace kvdb::storage
