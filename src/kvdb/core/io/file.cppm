export module kvdb.core.io.file;

import std;
import kvdb.core.io.io_uring;
import kvdb.core.coro.task;
import kvdb.core.coro.awaiter.io_awaiter;

export namespace kvdb::core::io {
// 导出文件打开模式的枚举类
enum class FileMode : std::uint8_t {
    Read,      // 只读
    Write,     // 只写（创建/覆盖）
    ReadWrite  // 读写（创建/覆盖）
};

// 导出一个封装文件描述符的RAII类
class File {
  public:
    // 构造函数：根据路径和模式打开文件
    File();
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

    // 异步写入 (offset = -1 表示追加)
    [[nodiscard]] auto write(std::span<const std::byte> buffer, std::int64_t offset)
        -> WriteAwaiter;

    [[nodiscard]] int getFd() const;

    // 获取文件大小
    [[nodiscard]] std::size_t get_size();

    // 静态方法，用于删除文件
    static void remove(const std::string& path);

  private:
    void open_if_needed();

    IOUring* ring_;
    int fd_{-1};  // 文件描述符
    std::string path_;
    FileMode mode_;
    std::atomic<std::size_t> file_size_{0};
};
}  // namespace kvdb::core::io