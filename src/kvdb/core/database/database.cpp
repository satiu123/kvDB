module kvdb.core;

import std;
import kvdb.logging.log;
import kvdb.storage;

using kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO,
    kvdb::logging::LOG_WARNING;

namespace kvdb::core {

Database::Database(std::string_view base_path) {
    auto data_path = std::filesystem::path(base_path) / "data";
    auto wal_path = data_path / "wal";
    sstables_path_ = (data_path / "sstables").string();

    std::filesystem::create_directories(wal_path);
    std::filesystem::create_directories(sstables_path_);

    wal_ = std::make_unique<storage::Wal>((wal_path / "kvdb.wal").string());
    recover();
}

void Database::recover() {
    // 1. Load existing SSTables
    if (std::filesystem::exists(sstables_path_)) {
        for (const auto& entry : std::filesystem::directory_iterator(sstables_path_)) {
            const auto& path = entry.path();
            if (entry.is_regular_file() && path.extension() == ".db" &&
                path.filename().string().starts_with("sstable_")) {
                auto sstable = std::make_unique<storage::SSTable>();
                if (sstable->open(path.string())) {
                    sstables_.push_back(std::move(sstable));
                }
            }
        }
    }
    // Sort SSTables by name (newest first)
    std::ranges::sort(sstables_,
                      [](const auto& a, const auto& b) { return a->getPath() > b->getPath(); });

    // 2. Replay the WAL to recover the memtable
    wal_->replay([this](const storage::WalRecord& record) {
        switch (record.getOpType()) {
            case storage::WalOpType::PUT: {
                data_[std::string(record.getKey())] = std::string(record.getValue());
                break;
            }
            case storage::WalOpType::REMOVE: {
                data_[std::string(record.getKey())] = "";  // Tombstone
                break;
            }
            case storage::WalOpType::CLEAR: {
                data_.clear();
                break;
            }
        }
        return true;
    });
    LOG_INFO()("Database recovered. {} SSTables loaded, {} records in memtable.", sstables_.size(),
               data_.size());
}

Database::~Database() {
    wal_->sync();
    wal_->close();
}

bool Database::put(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!wal_->appendPut(key, value)) {
        LOG_ERROR()("Failed to write to WAL: PUT key={}", key);
        return false;
    }

    data_[std::string(key)] = value;
    LOG_DEBUG()("PUT successful: key={}, value={}", key, value);

    if (data_.size() >= memtable_flush_threshold_) {
        LOG_INFO()("MemTable is full. Freezing and creating a new one.");
        immutable_memtable_ =
            std::make_unique<std::map<std::string, std::string>>(std::move(data_));
        data_.clear();

        std::string sstable_path = (std::filesystem::path(sstables_path_) /
                                    ("sstable_" + std::to_string(sstable_counter_++) + ".db"))
                                       .string();
        if (storage::SSTable::buildFrom(sstable_path, *immutable_memtable_)) {
            LOG_INFO()("Successfully flushed memtable to {}", sstable_path);
            immutable_memtable_.reset();
            auto sstable = std::make_unique<storage::SSTable>();
            if (sstable->open(sstable_path)) {
                sstables_.insert(sstables_.begin(), std::move(sstable));
            }
        } else {
            LOG_ERROR()("Failed to flush memtable to {}", sstable_path);
        }
    }

    return true;
}

std::optional<std::string> Database::get_locked(std::string_view key) const {
    auto it = data_.find(std::string(key));
    if (it != data_.end()) {
        return it->second.empty() ? std::nullopt : std::optional(it->second);
    }

    if (immutable_memtable_) {
        auto im_it = immutable_memtable_->find(std::string(key));
        if (im_it != immutable_memtable_->end()) {
            return im_it->second.empty() ? std::nullopt : std::optional(im_it->second);
        }
    }

    for (const auto& sstable : sstables_) {
        auto value = sstable->find(key);
        if (value) {
            return value->empty() ? std::nullopt : value;
        }
    }

    return std::nullopt;
}

std::optional<std::string> Database::get(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_locked(key);
}

bool Database::remove(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!get_locked(key).has_value()) {
        return false;
    }

    if (!wal_->appendRemove(key)) {
        LOG_ERROR()("Failed to write to WAL: REMOVE key={}", key);
        return false;
    }

    data_[std::string(key)] = "";
    LOG_DEBUG()("REMOVE successful (tombstone): key={}", key);

    return true;
}

std::size_t Database::size() const {
    return keys().size();
}

void Database::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!wal_->appendClear()) {
        LOG_ERROR()("Failed to write to WAL: CLEAR");
        return;
    }

    data_.clear();
    immutable_memtable_.reset();
    sstables_.clear();

    if (std::filesystem::exists(sstables_path_)) {
        for (const auto& entry : std::filesystem::directory_iterator(sstables_path_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".db" &&
                entry.path().filename().string().starts_with("sstable_")) {
                std::filesystem::remove(entry.path());
            }
        }
    }
    LOG_DEBUG()("CLEAR successful");
}

bool Database::exists(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_locked(key).has_value();
}

std::vector<std::string> Database::keys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, std::string> all_data;

    for (const auto& sstable : std::ranges::reverse_view(sstables_)) {
        auto sstable_data = sstable->readAll();
        for (const auto& [key, value] : sstable_data) {
            all_data[key] = value;
        }
    }
    if (immutable_memtable_) {
        for (const auto& [key, value] : *immutable_memtable_) {
            all_data[key] = value;
        }
    }
    for (const auto& [key, value] : data_) {
        all_data[key] = value;
    }

    std::vector<std::string> keys;
    for (const auto& [key, value] : all_data) {
        if (!value.empty()) {
            keys.push_back(key);
        }
    }
    return keys;
}

void Database::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO()("Starting compaction...");

    if (sstables_.size() <= 1 && immutable_memtable_ == nullptr) {
        LOG_INFO()("Not enough data to compact.");
        return;
    }

    std::map<std::string, std::string> all_data;
    for (const auto& sstable : std::ranges::reverse_view(sstables_)) {
        auto sstable_data = sstable->readAll();
        for (const auto& [key, value] : sstable_data) {
            all_data[key] = value;
        }
    }
    if (immutable_memtable_) {
        for (const auto& [key, value] : *immutable_memtable_) {
            all_data[key] = value;
        }
    }
    for (const auto& [key, value] : data_) {
        all_data[key] = value;
    }

    std::erase_if(all_data, [](const auto& item) {
        auto const& [key, value] = item;
        return value.empty();
    });

    std::string new_sstable_path =
        (std::filesystem::path(sstables_path_) /
         ("sstable_compacted_" + std::to_string(sstable_counter_++) + ".db"))
            .string();
    if (storage::SSTable::buildFrom(new_sstable_path, all_data)) {
        LOG_INFO()("Compaction successful. New SSTable: {}", new_sstable_path);

        std::vector<std::string> old_paths;
        for (const auto& sstable : sstables_) {
            old_paths.push_back(sstable->getPath());
        }

        sstables_.clear();
        data_.clear();
        immutable_memtable_.reset();

        for (const auto& path : old_paths) {
            std::filesystem::remove(path);
        }

        auto new_sstable = std::make_unique<storage::SSTable>();
        if (new_sstable->open(new_sstable_path)) {
            sstables_.push_back(std::move(new_sstable));
        }

    } else {
        LOG_ERROR()("Compaction failed.");
    }
}

void Database::setMemtableFlushThreshold(std::size_t threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    memtable_flush_threshold_ = threshold;
}

}  // namespace kvdb::core
