export module IOUring;

import std;

// 导出完成事件的结果结构体
// 调用者可以通过这个结构体获取异步操作的结果
export struct CompletionResult {
    std::int32_t result;      // 操作结果，例如读取的字节数或错误码
    std::uint64_t user_data;  // 与提交请求时关联的用户数据
};

// 导出IOUring类，封装io_uring的核心功能
export class IOUring {
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
    // user_data: 与此请求关联的用户自定义数据，会在完成事件中返回
    void submit_read(int fd, std::span<std::byte> buffer, std::uint64_t offset,
                     std::uint64_t user_data);

    // 提交一个写请求到队列
    // fd: 文件描述符
    // buffer: 要写入文件的数据缓冲区
    // offset: 文件写入的偏移量
    // user_data: 与此请求关联的用户自定义数据，会在完成事件中返回
    void submit_write(int fd, std::span<const std::byte> buffer, std::uint64_t offset,
                      std::uint64_t user_data);

    // 等待并获取一个完成事件
    // 这个函数会阻塞，直到至少有一个IO操作完成
    [[nodiscard]] auto wait_for_completion() -> CompletionResult;

  private:
    // 实际提交所有准备好的请求到内核
    void submit_requests();

    // 指向io_uring实例的指针
    // 使用void*是为了避免在接口文件中包含<liburing.h>
    void* ring_{nullptr};

    // 标记实例是否成功初始化
    bool initialized_{false};
};
