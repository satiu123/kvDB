module kvdb.storage.manager.snapshot_manager;
import kvdb.logging.log;
using kvdb::storage::SnapshotManager, kvdb::storage::SnapshotConfig, kvdb::logging::LOG_INFO,
    kvdb::logging::LOG_ERROR, kvdb::logging::LOG_WARNING;

namespace kvdb::storage {
SnapshotManager::SnapshotManager(Wal& wal, Snapshot& snapshot)
    : wal_(wal), snapshot_(snapshot), last_snapshot_time_(std::chrono::steady_clock::now()) {}

bool SnapshotManager::createSnapshot(const std::unordered_map<std::string, std::string>& data) {
    LOG_INFO()("开始创建数据快照...");

    // TODO: 获取当前WAL文件大小或偏移量
    std::uint64_t wal_offset = 0;

    if (!snapshot_.create(data, wal_offset)) {
        LOG_ERROR()("创建快照失败");
        return false;
    }

    // 快照创建成功后，可以截断WAL文件
    if (!wal_.truncate()) {
        LOG_WARNING()("截断WAL文件失败，但快照已创建成功");
    }

    // 重置计数器和时间
    resetCounters();

    LOG_INFO()("快照创建成功");
    return true;
}

void SnapshotManager::setConfig(const SnapshotConfig& config) {
    config_ = config;
    LOG_INFO()("快照配置已更新");
}

const SnapshotConfig& SnapshotManager::getConfig() const {
    return config_;
}

bool SnapshotManager::hasSnapshot() const {
    return snapshot_.exists();
}

void SnapshotManager::recordOperation() {
    ++operations_since_snapshot_;
}

bool SnapshotManager::checkAutoSnapshot(const std::unordered_map<std::string, std::string>& data) {
    if (!config_.auto_snapshot_enabled) {
        return false;
    }

    bool should_snapshot = false;

    // 检查操作数量阈值
    if (operations_since_snapshot_ >= config_.operation_count_threshold) {
        LOG_INFO()("操作数量达到阈值 ({})，准备创建快照", config_.operation_count_threshold);
        should_snapshot = true;
    }

    // 检查时间间隔
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - last_snapshot_time_);
    if (elapsed >= config_.time_interval) {
        LOG_INFO()("时间间隔达到阈值 ({} 分钟)，准备创建快照", config_.time_interval.count());
        should_snapshot = true;
    }

    // TODO: 检查WAL文件大小阈值
    // 需要扩展WAL类来提供文件大小信息

    if (should_snapshot) {
        return createSnapshot(data);
    }

    return false;
}

void SnapshotManager::resetCounters() {
    operations_since_snapshot_ = 0;
    last_snapshot_time_ = std::chrono::steady_clock::now();
}
}  // namespace kvdb::storage