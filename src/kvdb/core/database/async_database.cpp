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
import kvdb.core.coro.task;

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

// 解析 SSTable 文件名，支持：
//  - 旧格式：sstable-000001.sst（视为 L0）
//  - 新格式：sstable-L<level>-000001.sst
namespace {

int get_sstable_level_from_name(const std::string& path_str) {
    const std::string prefix = "sstable-L";
    std::string filename = std::filesystem::path(path_str).filename().string();
    if (filename.starts_with(prefix)) {
        auto dash_after_level = filename.find('-', prefix.size());
        if (dash_after_level != std::string::npos) {
            std::string level_str =
                filename.substr(prefix.size(), dash_after_level - prefix.size());
            try {
                return std::stoi(level_str);
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;  // 旧格式视为 L0
}

int get_sstable_number(const std::string& path_str) {
    std::string filename = std::filesystem::path(path_str).filename().string();
    const std::string new_prefix = "sstable-L";
    auto dot = filename.rfind('.');
    if (filename.starts_with(new_prefix)) {
        auto dash_after_level = filename.find('-', new_prefix.size());
        if (dash_after_level != std::string::npos && dot != std::string::npos &&
            dash_after_level + 1 < dot) {
            std::string num_str = filename.substr(dash_after_level + 1, dot - dash_after_level - 1);
            try {
                return std::stoi(num_str);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }
    // 旧格式：sstable-000001.sst
    auto first_dash = filename.find_first_of('-');
    if (first_dash == std::string::npos || dot == std::string::npos || first_dash + 1 >= dot) {
        return 0;
    }
    std::string number_str = filename.substr(first_dash + 1, dot - first_dash - 1);
    try {
        return std::stoi(number_str);
    } catch (...) {
        return 0;
    }
}
}  // namespace
AsyncDatabase::AsyncDatabase(std::string_view base_path)
    : ring_(std::make_unique<io::IOUring>(1024)) {
    // 将传入路径归一化为绝对路径：如果是相对路径，则以当前工作目录为基准
    std::filesystem::path input(base_path);
    std::filesystem::path abs =
        input.is_absolute() ? input : (std::filesystem::current_path() / input);
    base_path_ = abs.lexically_normal().string();

    // 基于绝对路径初始化 WAL / Manifest 与 SSTables 目录
    wal_ = std::make_unique<storage::AsyncWal>(*ring_, base_path_);
    manifest_ = std::make_unique<database::AsyncManifestFile>(*ring_, base_path_);
    std::filesystem::path path(base_path_);
    sstables_path_ = path / "sstables";
    std::filesystem::create_directories(sstables_path_);
    LOG_INFO()("SSTables 目录已创建或已存在: {}", sstables_path_);
    LOG_INFO()("异步数据库已在 '{}' 初始化", base_path_);
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
    // 排序：按 level 升序（L0 优先），同 level 内按编号降序（新的在前）
    std::ranges::sort(sstables_, [](const auto& a, const auto& b) {
        int la = get_sstable_level_from_name(a->getPath());
        int lb = get_sstable_level_from_name(b->getPath());
        if (la != lb)
            return la < lb;
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
        // 在将 MemTable 刷入 SSTable 前，确保 WAL 已持久化（组提交+fdatasync/fsync）
        bool synced = co_await wal_->async_sync();
        if (!synced) {
            LOG_ERROR()("在刷写 MemTable 前，同步 WAL 到磁盘失败");
            co_return false;
        }
        LOG_INFO()("MemTable 达到刷写阈值 ({})，准备刷写到 SSTable...", flush_threshold_);
        co_await flush_memtable_to_sstable();
    }

    co_return true;
}

// 旧的 WAL 批量缓冲已移除，统一由 AsyncWal 管理

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
        // 刷表前确保 WAL 持久化
        bool synced = co_await wal_->async_sync();
        if (!synced) {
            LOG_ERROR()("在刷写 MemTable 前，同步 WAL 到磁盘失败");
            co_return false;
        }
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

    // 1. 生成新的SSTable文件名（L0 内按级别单独编号）
    int max_sstable_num = 0;
    if (!manifest_data_.sstables[0].empty()) {
        for (const auto& p : manifest_data_.sstables[0]) {
            max_sstable_num = std::max(max_sstable_num, get_sstable_number(p));
        }
    }
    std::filesystem::path level_dir = std::filesystem::path(sstables_path_) / "L0";
    std::filesystem::create_directories(level_dir);
    std::filesystem::path sstable_path =
        level_dir / std::format("sstable-L0-{:06d}.sst", max_sstable_num + 1);
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
        } else {
            // 数据已安全持久化到 SSTable 且 Manifest 已更新，截断 WAL 以避免下次启动重放
            wal_->truncate();
            LOG_DEBUG()("MANIFEST 更新成功，WAL 已截断");
        }
    } else {
        LOG_ERROR()("构建 SSTable '{}' 失败", sstable_path.string());
    }

    immutable_memtable_.reset();
    LOG_INFO()("MemTable 刷写完成");
    // 如果 SSTable 数量超过最大限制，则触发压缩合并
    // if (sstables_.size() > max_sstable_num_) {
    //     LOG_INFO()("SSTable 数量超过最大限制（{}），触发压缩合并", max_sstable_num_);
    //     co_await compact_sstables();
    // }
    co_return;
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

Task<void> AsyncDatabase::compact_sstables() {
    LOG_INFO()("开始执行 SSTable 合并（压缩）...");
    // 若 SSTable 数量 <= 1，无需合并
    if (sstables_.size() <= 1) {
        LOG_INFO()("SSTable 数量为 {}，无需合并", sstables_.size());
        co_return;
    }

    // 读取所有现存 SSTable（从新到旧），构建最终键值视图（最新值覆盖旧值）
    OrderedKVMap merged;
    for (const auto& sst : sstables_) {
        auto all = co_await sst->readAll();
        for (const auto& [k, v] : all) {
            // sstables_ 已按新->旧排序，首次出现即为最新值
            if (!merged.contains(k)) {
                merged.emplace(k, v);
            }
        }
    }

    // 去除删除标记（空字符串）
    for (auto it = merged.begin(); it != merged.end();) {
        if (it->second.empty()) {
            it = merged.erase(it);
        } else {
            ++it;
        }
    }

    // 如果合并后为空，则清理现有文件并更新 Manifest
    if (merged.empty()) {
        LOG_INFO()("合并结果为空，将清理所有 SSTable 文件");
        // 删除磁盘上的旧文件
        for (const auto& p : manifest_data_.sstables[0]) {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
        // 仅移除内存中的 L0 表，保留其他层级
        sstables_.erase(
            std::ranges::remove_if(
                sstables_,
                [](const auto& s) { return get_sstable_level_from_name(s->getPath()) == 0; })
                .begin(),
            sstables_.end());
        manifest_data_.sstables[0].clear();
        auto store_result = co_await manifest_->async_store(manifest_data_);
        if (!store_result) {
            LOG_ERROR()("保存 MANIFEST 失败: {}", store_result.error());
        }
        LOG_INFO()("SSTable 合并完成（结果为空）");
        co_return;
    }

    // 生成新的 SSTable 文件名（使用比当前最大编号更大的编号）
    // 将 L0 全量合并输出到 L1，按 L1 独立编号
    int max_num = 0;
    for (const auto& p : manifest_data_.sstables[1]) {
        max_num = std::max(max_num, get_sstable_number(p));
    }
    std::filesystem::path level1_dir = std::filesystem::path(sstables_path_) / "L1";
    std::filesystem::create_directories(level1_dir);
    std::filesystem::path new_path = level1_dir / std::format("sstable-L1-{:06d}.sst", max_num + 1);

    // 构建新的合并后 SSTable
    bool build_ok = co_await storage::SSTable::buildFrom(*ring_, new_path.string(), merged);
    if (!build_ok) {
        LOG_ERROR()("构建合并后的 SSTable '{}' 失败", new_path.string());
        co_return;
    }

    // 打开新表
    auto new_sst = std::make_unique<storage::SSTable>(*ring_);
    if (!(co_await new_sst->open(new_path.string()))) {
        LOG_ERROR()("打开新建的合并 SSTable '{}' 失败", new_path.string());
        co_return;
    }

    // 删除旧文件，并更新内存与 Manifest
    for (const auto& p : manifest_data_.sstables[0]) {
        std::error_code ec;
        std::filesystem::remove(p, ec);
    }
    // 从内存中仅移除 L0 表
    sstables_.erase(
        std::ranges::remove_if(
            sstables_, [](const auto& s) { return get_sstable_level_from_name(s->getPath()) == 0; })
            .begin(),
        sstables_.end());
    sstables_.push_back(std::move(new_sst));
    // 重新排序，确保查询顺序正确
    std::ranges::sort(sstables_, [](const auto& a, const auto& b) {
        int la = get_sstable_level_from_name(a->getPath());
        int lb = get_sstable_level_from_name(b->getPath());
        if (la != lb)
            return la < lb;
        return get_sstable_number(a->getPath()) > get_sstable_number(b->getPath());
    });
    manifest_data_.sstables[0].clear();
    manifest_data_.sstables[1].push_back(new_path.string());
    manifest_data_.last_wal_sequence_number = wal_->getLastSequenceNumber();
    auto store_result = co_await manifest_->async_store(manifest_data_);
    if (!store_result) {
        LOG_ERROR()("保存 MANIFEST 失败: {}", store_result.error());
    }

    LOG_INFO()("SSTable 合并完成，新文件: '{}'", new_path.string());
}

}  // namespace kvdb::core