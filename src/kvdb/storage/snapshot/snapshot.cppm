export module kvdb.storage.snapshot;

import std;

export namespace kvdb::storage {

/**
 * @brief 快照文件格式版本
 */
constexpr std::uint32_t SNAPSHOT_VERSION = 1;

/**
 * @brief 快照文件魔数，用于文件格式验证
 */
constexpr std::uint32_t SNAPSHOT_MAGIC = 0x4B564442;  // "KVDB"

/**
 * @brief 快照文件头结构
 */
struct SnapshotHeader {
    std::uint32_t magic;         // 魔数
    std::uint32_t version;       // 版本号
    std::uint64_t timestamp;     // 创建时间戳
    std::uint64_t record_count;  // 记录数量
    std::uint64_t wal_offset;    // 对应的WAL偏移量
};

/**
 * @brief 快照管理器类
 *
 * 负责创建、读取和管理数据库快照
 */
class Snapshot {
  public:
    /**
     * @brief 构造函数
     * @param snapshot_path 快照文件路径
     */
    explicit Snapshot(std::string_view snapshot_path);

    /**
     * @brief 析构函数
     */
    ~Snapshot() = default;

    // 禁用拷贝和移动
    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot(Snapshot&&) = delete;
    Snapshot& operator=(Snapshot&&) = delete;

    /**
     * @brief 创建快照
     * @param data 要保存的数据
     * @param wal_offset 当前WAL偏移量
     * @return 是否成功
     */
    bool create(const std::unordered_map<std::string, std::string>& data,
                std::uint64_t wal_offset = 0);

    /**
     * @brief 从快照文件恢复数据
     * @param data 用于存储恢复数据的容器
     * @param wal_offset 返回快照对应的WAL偏移量
     * @return 是否成功
     */
    bool restore(std::unordered_map<std::string, std::string>& data, std::uint64_t& wal_offset);

    /**
     * @brief 检查快照文件是否存在
     * @return 文件是否存在
     */
    bool exists() const;

    /**
     * @brief 获取快照文件信息
     * @return 快照头信息，如果文件不存在或损坏则返回nullopt
     */
    std::optional<SnapshotHeader> getHeader() const;

    /**
     * @brief 删除快照文件
     * @return 是否成功
     */
    bool remove();

    /**
     * @brief 获取快照文件路径
     */
    const std::string& getPath() const {
        return snapshot_path_;
    }

  private:
    std::string snapshot_path_;

    /**
     * @brief 验证快照文件头
     * @param header 文件头
     * @return 是否有效
     */
    bool validateHeader(const SnapshotHeader& header) const;

    /**
     * @brief 写入字符串到文件
     * @param file 文件流
     * @param str 字符串
     * @return 是否成功
     */
    bool writeString(std::ofstream& file, const std::string& str) const;

    /**
     * @brief 从文件读取字符串
     * @param file 文件流
     * @param str 输出字符串
     * @return 是否成功
     */
    bool readString(std::ifstream& file, std::string& str) const;
};

}  // namespace kvdb::storage
