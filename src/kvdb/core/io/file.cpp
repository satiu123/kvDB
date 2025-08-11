module;

// 在模块实现文件中包含底层头文件
#include <fcntl.h>
#include <unistd.h>

#include <system_error>

module kvdb.core.io.file;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.coro.task;
import kvdb.core.coro.awaiter.io_awaiter;

namespace kvdb::core::io {
File::File(IOUring& ring, std::string_view path, FileMode mode)
    : ring_(&ring), path_(path), mode_(mode) {}

File::File() : ring_(nullptr), mode_(FileMode::Read) {}

void File::open_if_needed() {
    if (fd_ != -1) {
        return;
    }

    int flags = 0;
    switch (mode_) {
        case FileMode::Read:
            flags = O_RDONLY;
            break;
        case FileMode::Write:
            flags = O_WRONLY | O_CREAT;  // 不再默认截断
            break;
        case FileMode::ReadWrite:
            flags = O_RDWR | O_CREAT;
            break;
    }

    fd_ = open(path_.c_str(), flags, S_IRUSR | S_IWUSR);
    if (fd_ == -1) {
        throw std::system_error(errno, std::generic_category(), "打开文件失败: " + path_);
    }

    // 打开文件后，获取其大小
    try {
        file_size_ = std::filesystem::file_size(path_);
    } catch (const std::filesystem::filesystem_error& e) {
        // 如果文件刚被创建，获取大小可能会失败，此时大小为0是正常的
        if (e.code() == std::errc::no_such_file_or_directory) {
            file_size_ = 0;
        } else {
            throw;  // 其他错误则重新抛出
        }
    }
}

// 析构函数实现
File::~File() {
    if (fd_ != -1) {
        close(fd_);
    }
}

// 移动构造函数实现
File::File(File&& other) noexcept
    : ring_(other.ring_),
      fd_(other.fd_),
      path_(std::move(other.path_)),
      mode_(other.mode_),
      file_size_(other.file_size_.load()) {  // 移动文件大小
    other.ring_ = nullptr;
    other.fd_ = -1;  // 将原对象的文件描述符设为无效
}

// 移动赋值运算符实现
File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        if (fd_ != -1) {
            close(fd_);
        }
        ring_ = other.ring_;
        fd_ = other.fd_;
        path_ = std::move(other.path_);
        mode_ = other.mode_;
        file_size_.store(other.file_size_.load());  // 移动文件大小
        other.ring_ = nullptr;
        other.fd_ = -1;
    }
    return *this;
}

// 异步读取
auto File::read(std::span<std::byte> buffer, std::uint64_t offset) -> ReadAwaiter {
    open_if_needed();
    return ring_->submit_read(fd_, buffer, offset);
}

// 异步写入
auto File::write(std::span<const std::byte> buffer, std::int64_t offset) -> WriteAwaiter {
    open_if_needed();

    std::uint64_t write_offset = 0;
    if (offset == -1) {
        // -1 表示追加写入，使用当前文件大小作为偏移量
        write_offset = file_size_.fetch_add(buffer.size());
    } else {
        write_offset = static_cast<std::uint64_t>(offset);
        // 如果是绝对位置写入，需要更新文件大小
        std::uint64_t new_size = write_offset + buffer.size();
        if (new_size > file_size_.load()) {
            file_size_.store(new_size);
        }
    }

    return ring_->submit_write(fd_, buffer, write_offset);
}

// 获取文件描述符
int File::getFd() const {
    return fd_;
}

std::size_t File::get_size() {
    open_if_needed();
    return file_size_.load();
}

// 静态方法：删除文件
void File::remove(const std::string& path) {
    if (::remove(path.c_str()) != 0) {
        // 如果文件不存在，我们不认为这是一个需要抛出异常的错误
        if (errno != ENOENT) {
            throw std::system_error(errno, std::generic_category(),
                                    "删除文件失败: " + path);
        }
    }
}
}  // namespace kvdb::core::io