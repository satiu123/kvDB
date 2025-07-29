module kvdb.storage.manager.recovery_manager;
import kvdb.logging.log;
import kvdb.storage.wal.wal_record;
using kvdb::logging::LOG_INFO, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_DEBUG;

namespace kvdb::storage {
RecoveryManager::RecoveryManager(Wal& wal, Snapshot& snapshot) : wal_(wal), snapshot_(snapshot) {}

bool RecoveryManager::recover(std::map<std::string, std::string>& data) {
    // 优先从快照恢复，如果失败则从WAL恢复
    if (snapshot_.exists()) {
        return recoverFromSnapshot(data);
    } else {
        return recoverFromWal(data);
    }
}

bool RecoveryManager::recoverFromSnapshot(std::map<std::string, std::string>& data) {
    LOG_INFO()("正在从快照恢复数据...");

    std::uint64_t wal_offset = 0;
    if (!snapshot_.restore(data, wal_offset)) {
        LOG_ERROR()("从快照恢复失败，尝试从WAL恢复");
        return recoverFromWal(data);
    }

    LOG_INFO()("从快照恢复数据成功，共恢复{}条记录，WAL偏移: {}", data.size(), wal_offset);

    // 快照恢复成功后，还需要重放WAL中的所有记录
    // 因为当前WAL实现没有偏移功能，我们重放整个WAL
    // 在快照创建时，WAL已经被截断，所以这里重放的是快照后的记录
    LOG_INFO()("重放快照后的WAL记录...");

    if (!replayWalRecords(data)) {
        LOG_ERROR()("WAL重放失败");
        return false;
    }

    LOG_INFO()("WAL重放成功，最终数据库大小: {}", data.size());
    return true;
}

bool RecoveryManager::recoverFromWal(std::map<std::string, std::string>& data) {
    LOG_INFO()("正在从WAL文件恢复数据...");

    // 如果WAL文件为空，则无需恢复
    if (wal_.isEmpty()) {
        LOG_INFO()("WAL文件为空，无需恢复");
        return true;
    }

    if (!replayWalRecords(data)) {
        LOG_ERROR()("从WAL恢复数据失败");
        return false;
    }

    LOG_INFO()("从WAL恢复数据成功，共恢复{}条记录", data.size());
    return true;
}

bool RecoveryManager::replayWalRecords(std::map<std::string, std::string>& data) {
    // 重放WAL中的所有记录
    bool success = wal_.replay([&data](const WalRecord& record) {
        switch (record.getOpType()) {
            case WalOpType::PUT: {
                data[record.getKey().data()] = record.getValue();
                LOG_DEBUG()("恢复PUT操作: key={}, value={}", record.getKey(), record.getValue());
                break;
            }
            case WalOpType::REMOVE: {
                data.erase(record.getKey().data());
                LOG_DEBUG()("恢复REMOVE操作: key={}", record.getKey());
                break;
            }
            case WalOpType::CLEAR: {
                data.clear();
                LOG_DEBUG()("恢复CLEAR操作");
                break;
            }
            default:
                LOG_ERROR()("未知的WAL记录类型: {}", static_cast<int>(record.getOpType()));
                return false;
        }
        return true;
    });

    return success;
}
}  // namespace kvdb::storage