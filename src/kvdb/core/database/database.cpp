module kvdb.core;
import std;
import kvdb.logging.log;
import kvdb.storage.manager.snapshot_manager;

using kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO,
    kvdb::logging::LOG_WARNING;

namespace kvdb::core {
Database::Database(std::string_view wal_path, std::string_view snapshot_path)
    : wal_(wal_path),
      snapshot_(snapshot_path),
      recovery_manager_(wal_, snapshot_),
      snapshot_manager_(wal_, snapshot_) {
    // 使用恢复管理器恢复数据
    if (!recovery_manager_.recover(data_)) {
        LOG_ERROR()("数据恢复失败");
    }
}

Database::~Database() {
    wal_.sync();   // 确保所有数据都已同步到磁盘
    wal_.close();  // 确保在析构时关闭WAL文件
}

bool Database::put(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. 先写WAL
    if (!wal_.appendPut(key, value)) {
        LOG_ERROR()("写入WAL失败: PUT key={}", key);
        return false;
    }

    // 2. 再修改内存数据
    data_[key.data()] = value.data();
    LOG_DEBUG()("PUT操作成功: key={}, value={}", key, value);

    // 3. 记录操作并检查是否需要自动快照
    snapshot_manager_.recordOperation();
    snapshot_manager_.checkAutoSnapshot(data_);

    return true;
}

std::optional<std::string> Database::get(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key.data());
    if (it != data_.end()) {
        LOG_DEBUG()("GET操作成功: key={}, value={}", key, it->second);
        return it->second;
    }
    return std::nullopt;
}

bool Database::remove(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查键是否存在
    auto it = data_.find(std::string(key));
    if (it == data_.end()) {
        LOG_DEBUG()("删除操作失败: 键不存在 key={}", key);
        return false;  // 键不存在
    }

    // 1. 先写WAL
    if (!wal_.appendRemove(key)) {
        LOG_ERROR()("写入WAL失败: REMOVE key={}", key);
        return false;
    }

    // 2. 再修改内存数据
    bool result = data_.erase(std::string(key)) > 0;
    LOG_DEBUG()("REMOVE操作成功: key={}", key);

    // 3. 记录操作并检查是否需要自动快照
    snapshot_manager_.recordOperation();
    snapshot_manager_.checkAutoSnapshot(data_);

    return result;
}

std::size_t Database::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

void Database::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. 先写WAL
    if (!wal_.appendClear()) {
        LOG_ERROR()("写入WAL失败: CLEAR");
        return;
    }

    // 2. 再修改内存数据
    data_.clear();
    LOG_DEBUG()("CLEAR操作成功");

    // 3. 记录操作并检查是否需要自动快照
    snapshot_manager_.recordOperation();
    snapshot_manager_.checkAutoSnapshot(data_);
}

bool Database::exists(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.contains(key.data());
}

std::vector<std::string> Database::keys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(data_.size());
    for (const auto& pair : data_) {
        keys.push_back(pair.first);
    }
    return keys;
}

// 创建快照
bool Database::createSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_manager_.createSnapshot(data_);
}

// 设置快照配置
void Database::setSnapshotConfig(const storage::SnapshotConfig&& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_manager_.setConfig(config);
}

// 获取快照配置
const storage::SnapshotConfig& Database::getSnapshotConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_manager_.getConfig();
}

// 检查是否存在快照文件
bool Database::hasSnapshot() const {
    return snapshot_manager_.hasSnapshot();
}
}  // namespace kvdb::core