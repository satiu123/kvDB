module;

#include <liburing.h>
#include <stdexcept>

module IOUring;

import std;

// IOUring类的构造函数
// 初始化io_uring实例
IOUring::IOUring(unsigned int queue_depth) {
    // 分配io_uring实例的内存
    ring_ = new io_uring;
    // 初始化io_uring环
    if (io_uring_queue_init(queue_depth, static_cast<io_uring*>(ring_), 0) < 0) {
        delete static_cast<io_uring*>(ring_);
        ring_ = nullptr;
        throw std::runtime_error("Failed to initialize io_uring");
    }
    initialized_ = true;
}

// IOUring类的析构函数
// 清理io_uring实例
IOUring::~IOUring() {
    if (initialized_) {
        // 退出io_uring队列
        io_uring_queue_exit(static_cast<io_uring*>(ring_));
        delete static_cast<io_uring*>(ring_);
        ring_ = nullptr;
    }
}

// 提交读请求
void IOUring::submit_read(int fd, std::span<std::byte> buffer, std::uint64_t offset, std::uint64_t user_data) {
    // 从io_uring获取一个提交队列条目(SQE)
    io_uring_sqe* sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
    if (!sqe) {
        // 如果获取失败，先提交现有请求再重试
        submit_requests();
        sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
        if (!sqe) {
            throw std::runtime_error("Failed to get submission queue entry");
        }
    }
    // 准备读操作的SQE
    io_uring_prep_read(sqe, fd, buffer.data(), buffer.size(), offset);
    // 设置用户数据，用于在完成时识别操作
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(user_data));
}

// 提交写请求
void IOUring::submit_write(int fd, std::span<const std::byte> buffer, std::uint64_t offset, std::uint64_t user_data) {
    // 从io_uring获取一个提交队列条目(SQE)
    io_uring_sqe* sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
    if (!sqe) {
        // 如果获取失败，先提交现有请求再重试
        submit_requests();
        sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
        if (!sqe) {
            throw std::runtime_error("Failed to get submission queue entry");
        }
    }
    // 准备写操作的SQE
    io_uring_prep_write(sqe, fd, buffer.data(), buffer.size(), offset);
    // 设置用户数据
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(user_data));
}

// 等待一个IO操作完成
auto IOUring::wait_for_completion() -> CompletionResult {
    // 提交所有待处理的请求
    submit_requests();

    io_uring_cqe* cqe;
    // 等待完成队列中的一个条目(CQE)
    if (io_uring_wait_cqe(static_cast<io_uring*>(ring_), &cqe) < 0) {
        throw std::runtime_error("Failed to wait for completion queue entry");
    }

    // 从CQE中提取结果和用户数据
    CompletionResult result{
        .result = cqe->res,
        .user_data = reinterpret_cast<std::uint64_t>(io_uring_cqe_get_data(cqe))
    };

    // 标记CQE为已处理
    io_uring_cqe_seen(static_cast<io_uring*>(ring_), cqe);

    return result;
}

// 提交所有准备好的请求到内核
void IOUring::submit_requests() {
    // 提交请求，返回值是提交的请求数量
    if (io_uring_submit(static_cast<io_uring*>(ring_)) < 0) {
        throw std::runtime_error("Failed to submit requests to io_uring");
    }
}
