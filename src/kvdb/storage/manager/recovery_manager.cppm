export module kvdb.storage.manager.recovery_manager;

import std;
import kvdb.storage.wal.wal;
import kvdb.storage.snapshot.snapshot;
export namespace kvdb {

/**
 * @brief 数据恢复管理器
 *
 * 负责协调快照和WAL的恢复过程，将恢复逻辑从Database类中分离出来
 */
class RecoveryManager {
  public:
    /**
     * @brief 构造函数
     * @param wal WAL实例引用
     * @param snapshot 快照实例引用
     */
    RecoveryManager(Wal& wal, Snapshot& snapshot);

    /**
     * @brief 恢复数据到指定的容器中
     * @param data 用于存储恢复数据的容器
     * @return 是否成功
     */
    bool recover(std::unordered_map<std::string, std::string>& data);

  private:
    Wal& wal_;
    Snapshot& snapshot_;

    /**
     * @brief 从WAL恢复数据
     * @param data 数据容器
     * @return 是否成功
     */
    bool recoverFromWal(std::unordered_map<std::string, std::string>& data);

    /**
     * @brief 从快照恢复数据
     * @param data 数据容器
     * @return 是否成功
     */
    bool recoverFromSnapshot(std::unordered_map<std::string, std::string>& data);

    /**
     * @brief 重放WAL记录到数据容器
     * @param data 数据容器
     * @param handler WAL记录处理函数
     * @return 是否成功
     */
    bool replayWalRecords(std::unordered_map<std::string, std::string>& data);
};

}  // namespace kvdb
