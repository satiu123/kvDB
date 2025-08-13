export module kvdb.core.io.io_uring;

import std;
import kvdb.core.coro.awaiter.io_awaiter;
import kvdb.core.types;
import kvdb.core.io.ring_api;
export namespace kvdb::core::io {
using kvdb::core::types::ByteSpan;
using kvdb::core::types::ConstByteSpan;

// 导出IOUring类，封装io_uring的核心功能
class IOUring : public ISubmitter {
  public:
    // 构造函数，初始化一个指定队列深度的io_uring实例
    explicit IOUring(unsigned int queue_depth);

    // 析构函数，清理资源
    ~IOUring();

    // 禁止拷贝和移动，因为IOUring实例管理着一个底层资源
    IOUring(const IOUring&) = delete;
    IOUring& operator=(const IOUring&) = delete;
    IOUring(IOUring&&) = delete;
    IOUring& operator=(IOUring&&) = delete;

    // 提交一个读请求到队列
    // fd: 文件描述符
    // buffer: 数据读取的目标缓冲区
    // offset: 文件读取的偏移量
    [[nodiscard]] auto submit_read(int fd, ByteSpan buffer, std::uint64_t offset) -> ReadAwaiter;

    // 提交一个写请求到队列
    // fd: 文件描述符
    // buffer: 要写入文件的数据缓冲区
    // offset: 文件写入的偏移量
    [[nodiscard]] auto submit_write(int fd, ConstByteSpan buffer, std::uint64_t offset)
        -> WriteAwaiter;

    // 提交一个无操作请求到队列
    [[nodiscard]] auto nop() -> NopAwaiter;

    // 等待并获取一个完成事件
    // 这个函数会阻塞，直到至少有一个IO操作完成
    void wait_for_completion();

    // 非阻塞/带超时的等待，timeout_ms=0 表示立即返回，>0 表示超时毫秒
    bool wait_for_completion_for(std::uint32_t timeout_ms);

    // 供 Awaiter 调用的内部方法
    void submit_read_request(int fd, ByteSpan buffer, std::uint64_t offset,
                             void* user_data) override;
    void submit_write_request(int fd, ConstByteSpan buffer, std::uint64_t offset,
                              void* user_data) override;
    void submit_nop_request(void* user_data) override;

  private:
    // 实际提交所有准备好的请求到内核
    void submit_requests();

    // 指向io_uring实例的指针
    // 使用void*是为了避免在接口文件中包含<liburing.h>
    void* ring_{nullptr};

    // 标记实例是否成功初始化
    bool initialized_{false};
};
}  // namespace kvdb::core::io