module kvdb.core;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.database.manifest;
import kvdb.core.coro.task;
import kvdb.core.database.async_manifest;
import kvdb.storage.wal.async_wal;
import kvdb.storage.wal.wal_record;
import kvdb.logging.log;
import kvdb.storage.sstable;
import kvdb.core.types;

using kvdb::core::coro::Task;
using kvdb::core::types::KeyView;
using kvdb::core::types::OrderedKVMap;
using kvdb::core::types::ValueView;
using kvdb::logging::LOG_DEBUG;
using kvdb::logging::LOG_ERROR;
using kvdb::logging::LOG_INFO;
using kvdb::logging::LOG_WARNING;
using kvdb::storage::WalOpType;

namespace kvdb::core {

// 从SSTable文件名（如 "sstable-000001.sst"）中提取编号
namespace {

int get_sstable_number(const std::string& filename) {
    auto first = filename.find_first_of('-');
    auto last = filename.find_last_of('.');
    if (first == std::string::npos || last == std::string::npos) {
        return 0;
    }
    std::string number_str = filename.substr(first + 1, last - first - 1);
    return std::stoi(number_str);
}
}  // namespace
AsyncDatabase::AsyncDatabase(std::string_view base_path)
    : ring_(std::make_unique<io::IOUring>(1024)),
      wal_(std::make_unique<storage::AsyncWal>(*ring_, base_path)),
      manifest_(std::make_unique<database::AsyncManifestFile>(*ring_, base_path)) {
    std::filesystem::path path(base_path);
    sstables_path_ = path / "sstables";
    std::filesystem::create_directories(sstables_path_);
    LOG_INFO()("异步数据库已在 '{}' 初始化", std::string(base_path));
}

auto AsyncDatabase::init() -> Task<void> {
    LOG_INFO()("开始数据库初始化...");
    // 1. 异步加载 Manifest
    LOG_DEBUG()("正在加载 MANIFEST 文件...");
    auto manifest_data_opt = co_await manifest_->async_load();
    if (manifest_data_opt) {
        manifest_data_ = std::move(manifest_data_opt.value());
        LOG_DEBUG()("MANIFEST 文件加载成功");
    } else {
        LOG_WARNING()("未找到 MANIFEST 文件或加载失败，将创建新的 MANIFEST");
    }

    // 2. 加载SSTables
    LOG_DEBUG()("正在加载 SSTable 文件...");
    for (const auto& [level, files] : manifest_data_.sstables) {
        for (const auto& file_path : files) {
            auto sstable = std::make_unique<storage::SSTable>(*ring_);
            if (co_await sstable->open(file_path)) {
                sstables_.push_back(std::move(sstable));
                LOG_DEBUG()("SSTable '{}' 加载成功", file_path);
            } else {
                LOG_ERROR()("打开 SSTable '{}' 失败", file_path);
            }
        }
    }
    // 按编号对SSTable进行排序，确保搜索顺序正确（新的在前）
    std::ranges::sort(sstables_, [](const auto& a, const auto& b) {
        return get_sstable_number(a->getPath()) > get_sstable_number(b->getPath());
    });

    std::uint64_t max_seq{manifest_data_.last_wal_sequence_number};
    // 3. 定义 WAL 重放逻辑
    LOG_DEBUG()("准备重放 WAL...");
    auto replay_handler = [this, &max_seq](const storage::WalRecord& record) {
        switch (record.getOpType()) {
            case WalOpType::PUT:
                memtable_[std::string(record.getKey())] = record.getValue();
                break;
            case WalOpType::REMOVE:
                memtable_[std::string(record.getKey())] = "";  // 使用空字符串表示删除
                break;
            case WalOpType::CLEAR:
                memtable_.clear();
                break;
        }
        max_seq = std::max(max_seq, record.getSequenceNumber());
        return true;
    };

    // 4. 异步重放 WAL
    bool replay_ok = co_await wal_->async_replay(replay_handler);
    if (!replay_ok) {
        LOG_ERROR()("WAL 重放失败。数据库可能处于不一致状态。");
        throw std::runtime_error("从 WAL 初始化数据库失败。");
    }
    LOG_INFO()("WAL 重放完成，最大序列号为 {}", max_seq);

    // 5. 更新数据库的序列号
    wal_->setCurrentSequenceNumber(max_seq);
    LOG_INFO()("数据库初始化完成");
}

auto AsyncDatabase::async_put(KeyView key, ValueView value) -> Task<bool> {
    bool wal_ok = co_await wal_->async_append_put(key, value);
    if (!wal_ok) {
        LOG_ERROR()("向 WAL 写入 PUT 操作失败，键: {}", key);
        co_return false;
    }

    memtable_[std::string(key)] = value;
    LOG_DEBUG()("写入键: {}, 值: {}", key, value);
    if (memtable_.size() >= flush_threshold_) {  // 当memtable大小达到阈值时
        LOG_INFO()("MemTable 达到刷写阈值 ({})，准备刷写到 SSTable...", flush_threshold_);
        co_await flush_memtable_to_sstable();
    }

    co_return true;
}

auto AsyncDatabase::async_get(KeyView key) -> Task<std::optional<std::string>> {
    // 首先在可变 MemTable 中查找
    if (auto it = memtable_.find(std::string(key)); it != memtable_.end()) {
        LOG_DEBUG()("在 MemTable 中找到键: {}, 值: {}", key, it->second);
        if (it->second.empty()) {  // 空字符串表示已删除
            co_return std::nullopt;
        }
        co_return it->second;
    }

    // 然后在不可变 MemTable 中查找
    if (immutable_memtable_) {
        if (auto it = immutable_memtable_->find(std::string(key));
            it != immutable_memtable_->end()) {
            LOG_DEBUG()("在不可变 MemTable 中找到键: {}, 值: {}", key, it->second);
            if (it->second.empty()) {  // 空字符串表示已删除
                co_return std::nullopt;
            }
            co_return it->second;
        }
    }

    // 最后在 SSTable 中查找
    for (const auto& sstable : sstables_) {
        auto val = co_await sstable->find(key);
        if (val) {
            LOG_DEBUG()("在 SSTable '{}' 中找到键: {}, 值: {}", sstable->getPath(), key, *val);
            if (val->empty()) {  // 空字符串表示已删除
                co_return std::nullopt;
            }
            co_return val;
        }
    }
    LOG_DEBUG()("未找到键: {}", key);
    co_return std::nullopt;
}


auto AsyncDatabase::async_remove(KeyView key) -> Task<bool> {
    auto exists = co_await async_get(key);
    if (!exists) {
        LOG_DEBUG()("尝试删除的键不存在: {}", key);
        co_return false;  // 如果键不存在，直接返回false
    }
    bool wal_ok = co_await wal_->async_append_remove(key);
    if (!wal_ok) {
        LOG_ERROR()("向 WAL 写入 REMOVE 操作失败，键: {}", key);
        co_return false;
    }
    memtable_[std::string(key)] = "";  // 标记为删除
    LOG_DEBUG()("已删除键: {}", key);
    if (memtable_.size() >= flush_threshold_) {  // 当memtable大小达到阈值时
        LOG_INFO()("MemTable 达到刷写阈值 ({})，准备刷写到 SSTable...", flush_threshold_);
        co_await flush_memtable_to_sstable();
    }
    co_return true;
}

Task<void> AsyncDatabase::flush_memtable_to_sstable() {
    LOG_INFO()("开始将 MemTable 刷写到 SSTable...");
    immutable_memtable_ = std::make_unique<OrderedKVMap>();
    std::swap(memtable_, *immutable_memtable_);
    LOG_DEBUG()("MemTable 已交换为不可变 MemTable，大小为 {}", immutable_memtable_->size());

    // 1. 生成新的SSTable文件名
    int max_sstable_num = 0;
    if (!sstables_.empty()) {
        max_sstable_num = get_sstable_number(sstables_.front()->getPath());
    }
    std::filesystem::path sstable_path = sstables_path_;
    sstable_path /= std::format("sstable-{:06d}.sst", max_sstable_num + 1);
    LOG_DEBUG()("新的 SSTable 文件名为: {}", sstable_path.string());

    // 2. 从不可变MemTable构建SSTable
    bool build_ok =
        co_await storage::SSTable::buildFrom(*ring_, sstable_path.string(), *immutable_memtable_);

    if (build_ok) {
        LOG_INFO()("SSTable '{}' 构建成功", sstable_path.string());
        // 3. 将新的SSTable添加到Manifest和SSTable列表中
        auto new_sstable = std::make_unique<storage::SSTable>(*ring_);
        co_await new_sstable->open(sstable_path.string());
        sstables_.insert(sstables_.begin(), std::move(new_sstable));  // 添加到最前面 (最新的)

        manifest_data_.sstables[0].push_back(sstable_path.string());
        manifest_data_.last_wal_sequence_number = wal_->getLastSequenceNumber();
        auto store_result = co_await manifest_->async_store(manifest_data_);
        if (!store_result) {
            LOG_ERROR()("保存 MANIFEST 失败: {}", store_result.error());
        }
        LOG_DEBUG()("MANIFEST 更新成功");
    } else {
        LOG_ERROR()("构建 SSTable '{}' 失败", sstable_path.string());
    }

    immutable_memtable_.reset();
    LOG_INFO()("MemTable 刷写完成");
}

Task<void> AsyncDatabase::printWALRecords() const {
    auto records = co_await wal_->getFormattedContent();
    if (records) {
        std::cout << "--- WAL 记录 ---" << std::endl;
        for (const auto& record : *records) {
            std::cout << record << std::endl;
        }
    } else {
        std::cerr << "获取WAL记录失败: " << records.error() << std::endl;
    }
    co_return;
}

Task<void> AsyncDatabase::printSSTables() const {
    std::cout << "--- SSTables 内容 ---" << std::endl;
    for (const auto& sstable : sstables_) {
        auto sstable_map = co_await sstable->readAll();
        std::println("SSTable：{}", sstable->getPath());
        for (const auto& [k, v] : sstable_map) {
            std::println("  {}：{}", k, v);
        }
    }
}

void AsyncDatabase::printManifest() const {
    std::cout << "--- Manifest 内容 ---" << std::endl;
    std::cout << "最后的 WAL 序列号: " << manifest_data_.last_wal_sequence_number << std::endl;
    std::cout << "SSTables:" << std::endl;
    for (const auto& [level, files] : manifest_data_.sstables) {
        std::cout << "  层级 " << level << ":" << std::endl;
        for (const auto& file : files) {
            std::cout << "    " << file << std::endl;
        }
    }
}

}  // namespace kvdb::core