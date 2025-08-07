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

// 构造函数实现
File::File(IOUring& ring, const std::string& path, FileMode mode) : ring_(&ring), path_(path) {
    int flags = 0;
    switch (mode) {
        case FileMode::Read:
            flags = O_RDONLY;
            break;
        case FileMode::Write:
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case FileMode::ReadWrite:
            flags = O_RDWR | O_CREAT | O_TRUNC;
            break;
    }

    // S_IRUSR | S_IWUSR 表示用户读写权限
    fd_ = open(path.c_str(), flags, S_IRUSR | S_IWUSR);
    if (fd_ == -1) {
        throw std::system_error(errno, std::generic_category(), "Failed to open file: " + path);
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
    : ring_(other.ring_), fd_(other.fd_), path_(std::move(other.path_)) {
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
        other.ring_ = nullptr;
        other.fd_ = -1;
    }
    return *this;
}

// 异步读取
ReadAwaiter File::read(std::span<std::byte> buffer, std::uint64_t offset) {
    return ring_->submit_read(fd_, buffer, offset);
}

// 异步写入
WriteAwaiter File::write(std::span<const std::byte> buffer, std::uint64_t offset) {
    return ring_->submit_write(fd_, buffer, offset);
}

// 获取文件描述符
int File::get_fd() const {
    return fd_;
}

// 静态方法：删除文件
void File::remove(const std::string& path) {
    if (::remove(path.c_str()) != 0) {
        // 如果文件不存在，我们不认为这是一个需要抛出异常的错误
        if (errno != ENOENT) {
            throw std::system_error(errno, std::generic_category(),
                                    "Failed to remove file: " + path);
        }
    }
}
