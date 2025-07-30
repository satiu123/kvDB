module kvdb.storage.wal;

import std;
import kvdb.logging.log;
using kvdb::logging::LOG_ERROR, kvdb::logging::LOG_DEBUG;
namespace kvdb::storage {

// 构造函数
Wal::Wal(std::string_view path) : path_(path) {
    open(false);  // 打开WAL文件，不截断
}

// 析构函数
Wal::~Wal() {
    close();
}

// 添加PUT操作记录
bool Wal::appendPut(std::string_view key, std::string_view value) {
    WalRecord record(WalOpType::PUT, key, value, ++sequence_number_);
    return appendRecord(record);
}

// 添加REMOVE操作记录
bool Wal::appendRemove(std::string_view key) {
    WalRecord record(WalOpType::REMOVE, key, "", ++sequence_number_);
    return appendRecord(record);
}

// 添加CLEAR操作记录
bool Wal::appendClear() {
    WalRecord record(WalOpType::CLEAR, "", "", ++sequence_number_);
    return appendRecord(record);
}

// 添加任意记录
bool Wal::appendRecord(const WalRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_open_ && !open(false)) {
        LOG_ERROR()("无法打开WAL文件: {}", path_);
        return false;
    }

    // 序列化记录
    auto data = record.serialize();

    // 移动到文件末尾
    file_.seekp(0, std::ios::end);

    // 写入数据
    file_.write(reinterpret_cast<const char*>(data.data()), data.size());

    // 检查写入是否成功
    if (file_.fail()) {
        LOG_ERROR()("写入WAL记录失败: {}", path_);
        return false;
    }

    // 强制刷新缓冲区到磁盘（保证持久性）
    return sync();
}

// 同步WAL文件到磁盘
bool Wal::sync() {
    if (!is_open_) {
        return false;
    }

    file_.flush();

    // 在Linux系统上，可以使用fsync系统调用确保数据写入磁盘
    // 但C++标准库没有直接提供这个功能，所以这里使用flush作为替代
    // 实际产品中应考虑使用平台特定的fsync方法

    return !file_.fail();
}

// 打开WAL文件
bool Wal::open(bool truncate) {
    // 关闭已打开的文件
    if (is_open_) {
        file_.close();
    }

    // 设置打开模式
    std::ios_base::openmode mode = std::ios::binary | std::ios::in | std::ios::out | std::ios::ate;

    if (truncate) {
        mode |= std::ios::trunc;  // 截断文件
    } else {
        // 检查文件是否存在，不存在则创建
        std::filesystem::path fs_path(path_);
        if (!std::filesystem::exists(fs_path)) {
            mode |= std::ios::trunc;
        }
    }

    // 打开文件
    file_.open(path_, mode);

    // 检查是否成功打开
    is_open_ = file_.is_open();

    if (!is_open_) {
        LOG_ERROR()("无法打开WAL文件: {}", path_);
    }

    return is_open_;
}

// 从头开始重放WAL文件
bool Wal::replay(const std::function<bool(const WalRecord&)>& handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_open_ && !open(false)) {
        LOG_ERROR()("无法打开WAL文件进行重放: {}", path_);
        return false;
    }

    auto current_pos = file_.tellg();
    // 移动到文件开头
    file_.seekg(0, std::ios::beg);

    std::uint64_t max_seq = 0;
    // 一条一条读取并处理记录
    while (true) {
        auto record_result = readNextRecord();
        if (!record_result) {
            if (record_result.error() != "到达WAL文件末尾") {
                LOG_ERROR()("{}", record_result.error());
            }
            break;  // 到达文件末尾或发生错误
        }
        const auto& record = record_result.value();
        // 调用处理器处理记录
        if (!handler(*record)) {
            LOG_ERROR()("处理WAL记录时失败");
            file_.seekg(current_pos);  // 恢复位置
            return false;
        }
        max_seq = std::max(max_seq, record->getSequenceNumber());
    }

    // 检查是否因为读取错误而退出
    if (file_.bad()) {
        LOG_ERROR()("读取WAL文件时发生错误: {}", path_);
        file_.seekg(current_pos);  // 恢复到原来的位置
        return false;
    }

    file_.clear();               // 清除eof等状态位
    file_.seekg(current_pos);    // 恢复到原来的位置
    sequence_number_ = max_seq;  // 更新序列号
    return true;
}

// 读取下一个记录
std::expected<std::unique_ptr<WalRecord>, std::string> Wal::readNextRecord() {
    if (!is_open_ || file_.peek() == std::ios::traits_type::eof()) {
        return std::unexpected("WAL文件未打开或已到达末尾");
    }

    // 1. 读取记录的总长度 (4字节)
    std::uint32_t total_size;
    file_.read(reinterpret_cast<char*>(&total_size), sizeof(total_size));
    if (file_.gcount() == 0) { // 正常到达文件末尾
        return std::unexpected("到达WAL文件末尾");
    }
    if (!file_ || file_.gcount() != sizeof(total_size)) {
        return std::unexpected("读取WAL记录长度失败，文件可能已损坏");
    }

    // 2. 读取记录的剩余部分
    // total_size 包含了长度本身的4个字节，所以剩余部分是 total_size - 4
    if (total_size < sizeof(total_size)) { // 防止下溢
        return std::unexpected("无效的WAL记录长度");
    }
    std::vector<std::uint8_t> record_data(total_size);
    // 将已经读取的长度信息写回向量的开头
    std::memcpy(record_data.data(), &total_size, sizeof(total_size));

    file_.read(reinterpret_cast<char*>(record_data.data() + sizeof(total_size)), total_size - sizeof(total_size));
    if (!file_ || file_.gcount() != (total_size - sizeof(total_size))) {
        return std::unexpected("读取WAL记录数据失败，文件可能已损坏");
    }

    // 3. 反序列化记录
    return WalRecord::deserialize(record_data);
}

// 截断WAL文件
bool Wal::truncate() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 关闭当前文件
    if (is_open_) {
        file_.close();
        is_open_ = false;
    }

    // 重新打开文件并截断
    return open(true);
}

// 检查WAL文件是否为空
bool Wal::isEmpty() const {
    if (!is_open_) {
        return true;
    }

    // 因为是const方法，但需要访问文件，所以去掉const限定
    auto* this_ptr = const_cast<Wal*>(this);

    // 保存当前文件位置
    std::fstream::pos_type current_pos = this_ptr->file_.tellg();

    // 移动到文件开头
    this_ptr->file_.seekg(0, std::ios::beg);

    // 检查是否立即到达文件末尾
    bool is_empty = (this_ptr->file_.peek() == std::ios::traits_type::eof());

    // 恢复文件位置
    this_ptr->file_.seekg(current_pos);

    return is_empty;
}

// 关闭WAL文件
void Wal::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    // 如果文件已打开，则关闭它
    if (is_open_) {
        file_.close();
        is_open_ = false;
    }
}

// 获取格式化的WAL内容
std::expected<std::vector<std::string>, std::string> Wal::getFormattedContent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_open_) {
        return std::unexpected("WAL文件未打开");
    }
    if (isEmpty()) {
        return std::unexpected("WAL文件为空");
    }
    std::vector<std::string> formatted_lines;
    auto* this_ptr = const_cast<Wal*>(this);
    std::fstream::pos_type current_pos = this_ptr->file_.tellg();
    this_ptr->file_.seekg(0, std::ios::beg);
    while (true) {
        auto record = this_ptr->readNextRecord();
        if (!record) {
            LOG_ERROR()("{}", record.error());
            break;
        }
        formatted_lines.emplace_back(record.value()->toString());
    }
    this_ptr->file_.seekg(current_pos);  // 恢复文件位置
    if (formatted_lines.empty()) {
        return std::unexpected("WAL文件内容格式化失败");
    }
    return formatted_lines;
}

// 获取最后的序列号
std::uint64_t Wal::getLastSequenceNumber() const {
    return sequence_number_.load();
}

// 设置当前序列号
void Wal::setCurrentSequenceNumber(std::uint64_t seq) {
    sequence_number_ = seq;
}
}  // namespace kvdb::storage
