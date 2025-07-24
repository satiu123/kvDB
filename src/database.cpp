#include "kvdb/database.h"

#include <cstddef>  // for size_t
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "kvdb/log.h"

namespace kvdb {

Database::Database(std::string_view wal_path) : wal_(wal_path) {
    // 从WAL中恢复数据
    recoverFromWal();
}

Database::~Database() {
    wal_.sync();   // 确保所有数据都已同步到磁盘
    wal_.close();  // 确保在析构时关闭WAL文件
}

// 从WAL中恢复数据
void Database::recoverFromWal() {
    LOG_INFO("正在从WAL文件恢复数据...");

    // 如果WAL文件为空，则无需恢复
    if (wal_.isEmpty()) {
        LOG_INFO("WAL文件为空，无需恢复");
        return;
    }

    // 重放WAL中的所有记录
    bool success = wal_.replay([this](const WalRecord& record) {
        switch (record.getOpType()) {
            // 直接操作内存数据，不再写WAL
            case WalOpType::PUT: {
                data_[record.getKey().data()] = record.getValue();
                LOG_DEBUG("恢复PUT操作: key={}, value={}", record.getKey(), record.getValue());
                break;
            }
            case WalOpType::REMOVE: {
                data_.erase(record.getKey().data());
                LOG_DEBUG("恢复REMOVE操作: key={}", record.getKey());
                break;
            }
            case WalOpType::CLEAR: {
                data_.clear();
                LOG_DEBUG("恢复CLEAR操作");
                break;
            }
            default:
                LOG_ERROR("未知的WAL记录类型: {}", static_cast<int>(record.getOpType()));
                return false;
        }
        return true;
    });

    if (success) {
        LOG_INFO("从WAL恢复数据成功，共恢复{}条记录", data_.size());
    } else {
        LOG_ERROR("从WAL恢复数据失败");
    }
}

bool Database::put(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. 先写WAL
    if (!wal_.appendPut(key, value)) {
        LOG_ERROR("写入WAL失败: PUT key={}", key);
        return false;
    }

    // 2. 再修改内存数据
    data_[key.data()] = value.data();
    LOG_DEBUG("PUT操作成功: key={}, value={}", key, value);
    return true;
}

std::optional<std::string> Database::get(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key.data());
    if (it != data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Database::remove(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查键是否存在
    auto it = data_.find(std::string(key));
    if (it == data_.end()) {
        LOG_DEBUG("删除操作失败: 键不存在 key={}", key);
        return false;  // 键不存在
    }

    // 1. 先写WAL
    if (!wal_.appendRemove(key)) {
        LOG_ERROR("写入WAL失败: REMOVE key={}", key);
        return false;
    }

    // 2. 再修改内存数据
    bool result = data_.erase(std::string(key)) > 0;
    LOG_DEBUG("REMOVE操作成功: key={}", key);
    return result;
}

size_t Database::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

void Database::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. 先写WAL
    if (!wal_.appendClear()) {
        LOG_ERROR("写入WAL失败: CLEAR");
        return;
    }

    // 2. 再修改内存数据
    data_.clear();
    LOG_DEBUG("CLEAR操作成功");
}

bool Database::exists(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.contains(key.data());
}

}  // namespace kvdb
