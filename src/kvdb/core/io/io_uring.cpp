module;

#include <liburing.h>

#include <stdexcept>

module kvdb.core.io.io_uring;

import std;
import kvdb.core.coro.awaiter.io_awaiter;
import kvdb.core.types;
namespace kvdb::core::io {
using kvdb::core::types::ByteSpan;
using kvdb::core::types::ConstByteSpan;

// IOUring类的构造函数
// 初始化io_uring实例
IOUring::IOUring(unsigned int queue_depth) : ring_(new io_uring) {
    // 初始化io_uring环
    if (io_uring_queue_init(queue_depth, static_cast<io_uring*>(ring_), 0) < 0) {
        delete static_cast<io_uring*>(ring_);
        ring_ = nullptr;
        throw std::runtime_error("初始化io_uring失败");
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
auto IOUring::submit_read(int fd, ByteSpan buffer, std::uint64_t offset) -> ReadAwaiter {
    return ReadAwaiter{this, fd, buffer, offset};
}

// 提交写请求
auto IOUring::submit_write(int fd, ConstByteSpan buffer, std::uint64_t offset) -> WriteAwaiter {
    return WriteAwaiter{this, fd, buffer, offset};
}

// 提交nop请求
auto IOUring::nop() -> NopAwaiter {
    return NopAwaiter{this};
}

// 等待一个IO操作完成
void IOUring::wait_for_completion() {
    // 提交所有待处理的请求
    submit_requests();

    io_uring_cqe* cqe;
    // 等待完成队列中的一个条目(CQE)
    if (io_uring_wait_cqe(static_cast<io_uring*>(ring_), &cqe) < 0) {
        throw std::runtime_error("等待完成队列条目失败");
    }

    // 从CQE中提取结果和用户数据
    void* user_data = io_uring_cqe_get_data(cqe);
    auto result = cqe->res;

    // 标记CQE为已处理
    io_uring_cqe_seen(static_cast<io_uring*>(ring_), cqe);

    // 恢复协程
    // 通过基类指针安全地调用
    auto* awaiter = static_cast<BaseAwaiter*>(user_data);
    if (awaiter) {
        awaiter->set_result(result);
        awaiter->get_handle().resume();
    }
}

bool IOUring::wait_for_completion_for(std::uint32_t timeout_ms) {
    // 提交所有待处理的请求
    submit_requests();

    io_uring_cqe* cqe = nullptr;
    int ret = 0;
    if (timeout_ms == 0) {
        // 立即返回的非阻塞检查
        ret = io_uring_peek_cqe(static_cast<io_uring*>(ring_), &cqe);
        if (ret < 0 || cqe == nullptr) {
            return false;  // 没有完成事件
        }
    } else {
        // 带超时等待
        __kernel_timespec ts{};
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = static_cast<long>((timeout_ms % 1000)) * 1000000L;
        ret = io_uring_wait_cqe_timeout(static_cast<io_uring*>(ring_), &cqe, &ts);
        if (ret < 0) {
            if (ret == -ETIME) {
                return false;  // 超时
            }
            throw std::runtime_error("等待完成队列条目失败");
        }
        if (cqe == nullptr) {
            return false;
        }
    }

    // 处理完成事件
    void* user_data = io_uring_cqe_get_data(cqe);
    auto result = cqe->res;
    io_uring_cqe_seen(static_cast<io_uring*>(ring_), cqe);
    auto* awaiter = static_cast<BaseAwaiter*>(user_data);
    if (awaiter) {
        awaiter->set_result(result);
        awaiter->get_handle().resume();
    }
    return true;
}

void IOUring::submit_read_request(int fd, ByteSpan buffer, std::uint64_t offset, void* user_data) {
    // 从io_uring获取一个提交队列条目(SQE)
    io_uring_sqe* sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
    if (!sqe) {
        // 如果获取失败，先提交现有请求再重试
        submit_requests();
        sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
        if (!sqe) {
            throw std::runtime_error("获取提交队列条目失败");
        }
    }
    // 准备读操作的SQE
    io_uring_prep_read(sqe, fd, buffer.data(), buffer.size(), offset);
    // 设置用户数据，用于在完成时识别操作
    io_uring_sqe_set_data(sqe, user_data);
}

void IOUring::submit_nop_request(void* user_data) {
    io_uring_sqe* sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
    if (!sqe) {
        submit_requests();
        sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
        if (!sqe) {
            throw std::runtime_error("获取提交队列条目失败");
        }
    }
    // 准备nop操作的SQE
    io_uring_prep_nop(sqe);
    io_uring_sqe_set_data(sqe, user_data);
}

void IOUring::submit_write_request(int fd, ConstByteSpan buffer, std::uint64_t offset,
                                   void* user_data) {
    // 从io_uring获取一个提交队列条目(SQE)
    io_uring_sqe* sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
    if (!sqe) {
        // 如果获取失败，先提交现有请求再重试
        submit_requests();
        sqe = io_uring_get_sqe(static_cast<io_uring*>(ring_));
        if (!sqe) {
            throw std::runtime_error("获取提交队列条目失败");
        }
    }
    // 准备写操作的SQE
    io_uring_prep_write(sqe, fd, buffer.data(), buffer.size(), offset);
    // 设置用户数据，用于在完成时识别操作
    io_uring_sqe_set_data(sqe, user_data);
}

// 提交所有准备好的请求到内核
void IOUring::submit_requests() {
    // 提交请求，返回值是提交的请求数量
    if (io_uring_submit(static_cast<io_uring*>(ring_)) < 0) {
        throw std::runtime_error("向io_uring提交请求失败");
    }
}

}  // namespace kvdb::core::io
