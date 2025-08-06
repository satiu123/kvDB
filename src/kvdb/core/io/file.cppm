export module File;

import std;
import IOUring;
import kvdb.core.coro.task;
import kvdb.core.coro.awaiter.io_awaiter;
// 导出文件打开模式的枚举类
export enum class FileMode : std::uint8_t {
    Read,      // 只读
    Write,     // 只写（创建/覆盖）
    ReadWrite  // 读写（创建/覆盖）
};

// 导出一个封装文件描述符的RAII类
export class File {
  public:
    // 构造函数：根据路径和模式打开文件
    File(IOUring& ring, const std::string& path, FileMode mode);

    // 析构函数：自动关闭文件描述符
    ~File();

    // 禁止拷贝，因为每个实例拥有唯一的文件描述符
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    // 允许移动，以转移文件描述符的所有权
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    // 异步读取
    [[nodiscard]] auto read(std::span<std::byte> buffer, std::uint64_t offset) -> ReadAwaiter;

    // 异步写入
    [[nodiscard]] auto write(std::span<const std::byte> buffer, std::uint64_t offset)
        -> WriteAwaiter;

    // 获取原始的文件描述符，用于传递给IOUring等API
    [[nodiscard]] int get_fd() const;

    // 静态方法，用于删除文件
    static void remove(const std::string& path);

  private:
    IOUring* ring_;
    int fd_{-1};  // 文件描述符
    std::string path_;
};
