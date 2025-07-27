module kvdb.storage.snapshot;

import std;
import kvdb.logging.log;
using kvdb::logging::LOG_INFO, kvdb::logging::LOG_ERROR;

namespace kvdb::storage {
Snapshot::Snapshot(std::string_view snapshot_path) : snapshot_path_(snapshot_path) {}

bool Snapshot::create(const std::unordered_map<std::string, std::string>& data,
                      std::uint64_t wal_offset) {
    LOG_INFO()("开始创建快照文件: {}", snapshot_path_);

    std::ofstream file(snapshot_path_, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR()("无法创建快照文件: {}", snapshot_path_);
        return false;
    }

    // 构造文件头
    SnapshotHeader header;
    header.magic = SNAPSHOT_MAGIC;
    header.version = SNAPSHOT_VERSION;
    header.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    header.record_count = data.size();
    header.wal_offset = wal_offset;

    // 写入文件头
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!file.good()) {
        LOG_ERROR()("写入快照文件头失败: {}", snapshot_path_);
        return false;
    }

    // 写入数据记录
    for (const auto& [key, value] : data) {
        // 写入键值对
        if (!writeString(file, key) || !writeString(file, value)) {
            LOG_ERROR()("写入快照数据失败: key={}", key);
            return false;
        }
    }

    file.close();
    if (!file.good()) {
        LOG_ERROR()("关闭快照文件失败: {}", snapshot_path_);
        return false;
    }

    LOG_INFO()("快照创建成功: {}, 记录数: {}, WAL偏移: {}", snapshot_path_, data.size(),
               wal_offset);
    return true;
}

bool Snapshot::restore(std::unordered_map<std::string, std::string>& data,
                       std::uint64_t& wal_offset) {
    LOG_INFO()("开始从快照恢复数据: {}", snapshot_path_);

    std::ifstream file(snapshot_path_, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR()("无法打开快照文件: {}", snapshot_path_);
        return false;
    }

    // 读取文件头
    SnapshotHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good()) {
        LOG_ERROR()("读取快照文件头失败: {}", snapshot_path_);
        return false;
    }

    // 验证文件头
    if (!validateHeader(header)) {
        LOG_ERROR()("快照文件头验证失败: {}", snapshot_path_);
        return false;
    }

    // 清空数据容器
    data.clear();
    data.reserve(header.record_count);

    // 读取数据记录
    for (std::uint64_t i = 0; i < header.record_count; ++i) {
        std::string key, value;
        if (!readString(file, key) || !readString(file, value)) {
            LOG_ERROR()("读取快照数据失败，记录索引: {}", i);
            return false;
        }
        data[key] = value;
    }

    wal_offset = header.wal_offset;

    LOG_INFO()("快照恢复成功: {}, 记录数: {}, WAL偏移: {}", snapshot_path_, data.size(),
               wal_offset);
    return true;
}

bool Snapshot::exists() const {
    return std::filesystem::exists(snapshot_path_);
}

std::optional<SnapshotHeader> Snapshot::getHeader() const {
    std::ifstream file(snapshot_path_, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    SnapshotHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good()) {
        return std::nullopt;
    }

    if (!validateHeader(header)) {
        return std::nullopt;
    }

    return header;
}

bool Snapshot::remove() {
    try {
        return std::filesystem::remove(snapshot_path_);
    } catch (const std::exception& e) {
        LOG_ERROR()("删除快照文件失败: {}, 错误: {}", snapshot_path_, e.what());
        return false;
    }
}

bool Snapshot::validateHeader(const SnapshotHeader& header) const {
    return header.magic == SNAPSHOT_MAGIC && header.version == SNAPSHOT_VERSION;
}

bool Snapshot::writeString(std::ofstream& file, const std::string& str) const {
    // 先写入字符串长度
    std::uint32_t length = str.size();
    file.write(reinterpret_cast<const char*>(&length), sizeof(length));
    if (!file.good()) {
        return false;
    }

    // 再写入字符串内容
    if (length > 0) {
        file.write(str.data(), length);
        if (!file.good()) {
            return false;
        }
    }

    return true;
}

bool Snapshot::readString(std::ifstream& file, std::string& str) const {
    // 先读取字符串长度
    std::uint32_t length;
    file.read(reinterpret_cast<char*>(&length), sizeof(length));
    if (!file.good()) {
        return false;
    }

    // 检查长度合理性（防止恶意文件）
    if (length > 1024 * 1024) {  // 限制单个字符串最大1MB
        LOG_ERROR()("字符串长度异常: {}", length);
        return false;
    }

    // 读取字符串内容
    if (length > 0) {
        str.resize(length);
        file.read(str.data(), length);
        if (!file.good()) {
            return false;
        }
    } else {
        str.clear();
    }

    return true;
}
}  // namespace kvdb::storage