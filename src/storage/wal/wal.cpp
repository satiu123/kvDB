#include "kvdb/storage/wal/wal.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include "kvdb/logging/log.h"

namespace kvdb {

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
    WalRecord record(WalOpType::PUT, key, value);
    return appendRecord(record);
}

// 添加REMOVE操作记录
bool Wal::appendRemove(std::string_view key) {
    WalRecord record(WalOpType::REMOVE, key);
    return appendRecord(record);
}

// 添加CLEAR操作记录
bool Wal::appendClear() {
    WalRecord record(WalOpType::CLEAR);
    return appendRecord(record);
}

// 添加任意记录
bool Wal::appendRecord(const WalRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_open_ && !open(false)) {
        LOG_ERROR("无法打开WAL文件: {}", path_);
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
        LOG_ERROR("写入WAL记录失败: {}", path_);
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
    std::ios_base::openmode mode = std::ios::binary | std::ios::in | std::ios::out;

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
        LOG_ERROR("无法打开WAL文件: {}", path_);
    }

    return is_open_;
}

// 从头开始重放WAL文件
bool Wal::replay(const std::function<bool(const WalRecord&)>& handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_open_ && !open(false)) {
        LOG_ERROR("无法打开WAL文件进行重放: {}", path_);
        return false;
    }

    // 移动到文件开头
    file_.seekg(0, std::ios::beg);

    // 一条一条读取并处理记录
    std::unique_ptr<WalRecord> record;
    while ((record = readNextRecord()) != nullptr) {
        // 调用处理器处理记录
        if (!handler(*record)) {
            LOG_ERROR("处理WAL记录时失败");
            return false;
        }
    }

    // 检查是否因为读取错误而退出
    if (file_.bad()) {
        LOG_ERROR("读取WAL文件时发生错误: {}", path_);
        return false;
    }

    return true;
}

// 读取下一个记录
std::unique_ptr<WalRecord> Wal::readNextRecord() {
    if (!is_open_) {
        return nullptr;
    }

    // 检查是否到达文件末尾
    if (file_.peek() == EOF) {
        return nullptr;
    }

    try {
        // 先读取记录头部以确定记录大小
        const size_t header_size = WalRecord::getHeaderSize();
        std::vector<uint8_t> header(header_size);

        // 读取头部
        file_.read(reinterpret_cast<char*>(header.data()), header.size());

        // 检查是否读取成功
        if (file_.fail()) {
            if (file_.eof()) {
                LOG_ERROR("读取WAL记录头部时遇到文件结束");
                return nullptr;
            }

            LOG_ERROR("读取WAL记录头部失败");
            return nullptr;
        }

        // 从头部解析记录总大小
        uint32_t total_size;
        std::memcpy(&total_size, header.data() + 13,
                    sizeof(total_size));  // 总大小在头部的第13个字节

        // 调整文件指针回到记录开头
        file_.seekg(-static_cast<int>(header_size), std::ios::cur);

        // 读取完整记录
        std::vector<uint8_t> record_data(total_size);
        file_.read(reinterpret_cast<char*>(record_data.data()), record_data.size());

        // 检查是否读取成功
        if (file_.fail()) {
            LOG_ERROR("读取完整WAL记录失败");
            return nullptr;
        }

        // 反序列化记录
        return WalRecord::deserialize(record_data);
    } catch (const std::exception& e) {
        LOG_ERROR("反序列化WAL记录时发生异常: {}", e.what());
        return nullptr;
    }
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
    bool is_empty = (this_ptr->file_.peek() == EOF);

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

}  // namespace kvdb
