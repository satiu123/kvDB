module kvdb.core;

import std;
import kvdb.logging.log;
import kvdb.storage;

using kvdb::logging::LOG_DEBUG, kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO;

namespace kvdb::core {

Database::Database(std::string_view base_path) {
    auto data_path = std::filesystem::path(base_path) / "data";
    auto wal_path = data_path / "wal";
    auto manifest_path = data_path / "manifest";
    sstables_path_ = (data_path / "sstables").string();

    std::filesystem::create_directories(wal_path);
    std::filesystem::create_directories(sstables_path_);
    std::filesystem::create_directories(manifest_path);

    wal_ = std::make_unique<storage::Wal>((wal_path / "kvdb.wal").string());
    manifest_ = std::make_unique<database::ManifestFile>(manifest_path.string());

    recover();
}

void Database::recover() {
    // 1. 加载 MANIFEST 文件
    auto manifest_result = manifest_->load();
    if (!manifest_result) {
        LOG_ERROR()("加载 MANIFEST 文件失败: {}", manifest_result.error());
        // Even if manifest fails to load, we can proceed with full WAL replay
    } else {
        manifest_data_ = std::move(manifest_result.value());
    }

    // 2. 加载SSTable
    for (const auto& [level, files] : manifest_data_.sstables) {
        for (const auto& file : files) {
            auto sstable = std::make_unique<storage::SSTable>();
            if (sstable->open((std::filesystem::path(sstables_path_) / file).string())) {
                sstables_.push_back(std::move(sstable));
            }
        }
    }
    // 按名称对SSTable进行排序（最新的在前）
    std::ranges::sort(sstables_,
                      [](const auto& a, const auto& b) { return a->getPath() > b->getPath(); });

    // 3. 重放WAL以恢复内存表
    std::uint64_t last_wal_seq = manifest_data_.last_wal_sequence_number;
    wal_->replay([this, last_wal_seq](const storage::WalRecord& record) {
        if (record.getSequenceNumber() <= last_wal_seq) {
            return true;  // Skip records that are already reflected in an SSTable
        }
        switch (record.getOpType()) {
            case storage::WalOpType::PUT: {
                data_[std::string(record.getKey())] = std::string(record.getValue());
                break;
            }
            case storage::WalOpType::REMOVE: {
                data_[std::string(record.getKey())] = "";  // 墓碑
                break;
            }
            case storage::WalOpType::CLEAR: {
                data_.clear();
                break;
            }
        }
        return true;
    });
    LOG_INFO()("数据库已恢复。加载了 {} 个SSTable，内存表中有 {} 条记录。", sstables_.size(),
               data_.size());
}

Database::~Database() {
    wal_->sync();
    wal_->close();
}

bool Database::put(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!wal_->appendPut(key, value)) {
        LOG_ERROR()("写入WAL失败: PUT key={}", key);
        return false;
    }

    data_[std::string(key)] = value;
    LOG_DEBUG()("PUT 成功: key={}, value={}", key, value);

    flushMemtableIfNeeded();

    return true;
}

std::optional<std::string> Database::get_locked(std::string_view key) const {
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second.empty() ? std::nullopt : std::optional(it->second);
    }

    if (immutable_memtable_) {
        auto im_it = immutable_memtable_->find(key);
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
        LOG_ERROR()("写入WAL失败: REMOVE key={}", key);
        return false;
    }

    data_[std::string(key)] = "";
    LOG_DEBUG()("REMOVE 成功 (墓碑): key={}", key);

    flushMemtableIfNeeded();

    return true;
}

std::size_t Database::size() const {
    return keys().size();
}

void Database::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!wal_->appendClear()) {
        LOG_ERROR()("写入WAL失败: CLEAR");
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

    // 清空并存储 MANIFEST
    manifest_data_ = database::Manifest{};
    manifest_->store(manifest_data_);

    LOG_DEBUG()("CLEAR 成功");
}

bool Database::exists(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_locked(key).has_value();
}
std::map<std::string, std::string, std::less<>> Database::get_all_data() const {
    std::map<std::string, std::string, std::less<>> all_data;

    // 从SSTables读取数据
    for (const auto& sstable : std::ranges::reverse_view(sstables_)) {
        auto sstable_data = sstable->readAll();
        for (const auto& [key, value] : sstable_data) {
            all_data[key] = value;
        }
    }

    // 从不可变内存表读取数据
    if (immutable_memtable_) {
        for (const auto& [key, value] : *immutable_memtable_) {
            all_data[key] = value;
        }
    }

    // 从可变内存表读取数据
    for (const auto& [key, value] : data_) {
        all_data[key] = value;
    }

    // 过滤掉空值
    std::erase_if(all_data, [](const auto& item) {
        auto const& [key, value] = item;
        return value.empty();
    });

    return all_data;
}
std::vector<std::string> Database::keys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> all_keys;
    std::map<std::string, std::string, std::less<>> all_data = get_all_data();
    all_keys.reserve(all_data.size());
    for (const auto& [key, value] : all_data) {
        all_keys.push_back(key);
    }
    return all_keys;
}

void Database::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO()("开始压缩...");

    if (sstables_.size() <= 1 && immutable_memtable_ == nullptr) {
        LOG_INFO()("没有足够的数据进行压缩。");
        return;
    }

    std::map<std::string, std::string, std::less<>> all_data = get_all_data();

    std::string new_sstable_path =
        (std::filesystem::path(sstables_path_) /
         ("sstable_compacted_" + std::to_string(sstable_counter_++) + ".db"))
            .string();
    if (storage::SSTable::buildFrom(new_sstable_path, all_data)) {
        LOG_INFO()("压缩成功。新的SSTable: {}", new_sstable_path);

        std::vector<std::string> old_paths;
        old_paths.reserve(sstables_.size());
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
        LOG_ERROR()("压缩失败。");
    }
}

void Database::setMemtableFlushThreshold(std::size_t threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    memtable_flush_threshold_ = threshold;
}

void Database::printWALRecords() const {
    auto records = wal_->getFormattedContent();
    if (records) {
        std::cout << "--- WAL Records ---" << std::endl;
        for (const auto& record : *records) {
            std::cout << record << std::endl;
        }
    } else {
        std::cerr << "获取WAL记录失败: " << records.error() << std::endl;
    }
}

void Database::printSSTables() const {
    std::cout << "--- SSTables Content ---" << std::endl;
    for (const auto& sstable : sstables_) {
        std::cout << "SSTable: " << sstable->getPath() << std::endl;
        auto all_data = sstable->readAll();
        for (const auto& [key, value] : all_data) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
    }
}

void Database::printManifest() const {
    std::cout << "--- Manifest Content ---" << std::endl;
    std::cout << "Last WAL Sequence Number: " << manifest_data_.last_wal_sequence_number
              << std::endl;
    std::cout << "SSTables:" << std::endl;
    for (const auto& [level, files] : manifest_data_.sstables) {
        std::cout << "  Level " << level << ":" << std::endl;
        for (const auto& file : files) {
            std::cout << "    " << file << std::endl;
        }
    }
}

void Database::flushMemtableIfNeeded() {
    if (data_.size() >= memtable_flush_threshold_) {
        LOG_INFO()("内存表已满。正在冻结并创建新的内存表。");
        immutable_memtable_ =
            std::make_unique<std::map<std::string, std::string, std::less<>>>(std::move(data_));
        data_.clear();

        std::string sstable_filename = "sstable_" + std::to_string(sstable_counter_++) + ".db";
        std::string sstable_path =
            (std::filesystem::path(sstables_path_) / sstable_filename).string();

        if (storage::SSTable::buildFrom(sstable_path, *immutable_memtable_)) {
            LOG_INFO()("成功将内存表刷写到 {}", sstable_path);
            immutable_memtable_.reset();
            auto sstable = std::make_unique<storage::SSTable>();
            if (sstable->open(sstable_path)) {
                sstables_.insert(sstables_.begin(), std::move(sstable));
            }

            // 更新并存储 MANIFEST
            manifest_data_.sstables[0].push_back(sstable_filename);
            manifest_data_.last_wal_sequence_number = wal_->getLastSequenceNumber();
            auto store_result = manifest_->store(manifest_data_);
            if (!store_result) {
                LOG_ERROR()("存储 MANIFEST 文件失败: {}", store_result.error());
            } else {
                // SSTable 和 MANIFEST 都已成功写入，现在可以安全地截断 WAL
                if (!wal_->truncate()) {
                    LOG_ERROR()("截断 WAL 文件失败");
                }
            }

        } else {
            LOG_ERROR()("将内存表刷写到 {} 失败", sstable_path);
        }
    }
}

}  // namespace kvdb::core
