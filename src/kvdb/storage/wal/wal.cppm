export module kvdb.storage.wal;

import std;
export import kvdb.storage.wal.wal_record;

export namespace kvdb::storage {

/**
 * @brief 预写式日志(Write-Ahead Log)类
 *
 * WAL用于确保数据库操作的持久性和一致性
 * 在修改内存中的数据前，先将操作记录到WAL文件
 * 这样在系统崩溃后可以通过重放WAL恢复数据
 */
class Wal {
  public:
    /**
     * @brief 构造函数
     * @param path WAL文件路径
     */
    explicit Wal(std::string_view path);

    /**
     * @brief 析构函数
     */
    ~Wal();
    /**
     * @brief 添加一条PUT操作记录到WAL
     * @param key 键
     * @param value 值
     * @return 是否成功
     */
    bool appendPut(std::string_view key, std::string_view value);

    /**
     * @brief 添加一条REMOVE操作记录到WAL
     * @param key 要删除的键
     * @return 是否成功
     */
    bool appendRemove(std::string_view key);

    /**
     * @brief 添加一条CLEAR操作记录到WAL
     * @return 是否成功
     */
    bool appendClear();

    /**
     * @brief 添加任意一条记录到WAL
     * @param record WAL记录
     * @return 是否成功
     */
    bool appendRecord(const WalRecord& record);

    /**
     * @brief 从头开始重放WAL文件
     * @param handler 处理每条记录的回调函数
     * @return 是否成功完成所有记录的重放
     */
    bool replay(const std::function<bool(const WalRecord&)>& handler);

    /**
     * @brief 截断WAL文件（删除所有内容）
     * @return 是否成功
     */
    bool truncate();

    /**
     * @brief 同步WAL文件到磁盘
     * @return 是否成功
     */
    bool sync();

    /**
     * @brief 检查WAL文件是否为空
     * @return 是否为空
     */
    bool isEmpty() const;

    /**
     * @brief 关闭WAL文件
     */
    void close();
    /**
     * @brief 获取格式化的WAL内容
     * @return 格式化的WAL内容字符串数组
     */
    std::expected<std::vector<std::string>, std::string> getFormattedContent() const;

  private:
    std::string path_;          // WAL文件路径
    std::fstream file_;         // WAL文件流
    mutable std::mutex mutex_;  // 互斥锁，用于保护并发访问
    bool is_open_ = false;      // WAL文件是否打开

    /**
     * @brief 打开WAL文件
     * @param truncate 是否截断文件
     * @return 是否成功
     */
    bool open(bool truncate = false);

    /**
     * @brief 读取下一个记录
     * @return 下一个记录，如果到达文件末尾或出错则返回nullptr
     */
    std::unique_ptr<WalRecord> readNextRecord();
};

}  // namespace kvdb::storage
