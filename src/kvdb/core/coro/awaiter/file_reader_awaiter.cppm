export module kvdb.core.coro.awaiter:file_reader;

import std;

namespace kvdb::core::coro
{
export class [[nodiscard]] FileReaderAwaiter
{
public:
    // 构造函数，接收文件名
    explicit FileReaderAwaiter(std::string filename) : filename_(std::move(filename)) {}

    // await_ready 总是返回 false，因为文件读取总是需要挂起
    bool await_ready() const noexcept { return false; }

    // await_suspend 是挂起协程并启动异步操作的地方
    void await_suspend(std::coroutine_handle<> handle)
    {
        // 在一个新线程中执行阻塞的文件I/O操作
        std::thread([this, handle]() {
            std::ifstream file(filename_);
            if (file)
            {
                std::stringstream buffer;
                buffer << file.rdbuf();
                result_ = buffer.str();
            }
            else
            {
                // 如果文件无法打开，可以设置一个错误状态或抛出异常
                // 为简单起见，这里我们只让结果为空
            }
            // 文件读完后，恢复协程
            handle.resume();
        }).detach(); // 分离线程，让它在后台运行
    }

    // await_resume 在协程恢复后调用，返回操作结果
    std::string await_resume() noexcept { return std::move(result_); }

private:
    std::string filename_;
    std::string result_;
};

// 辅助函数，使调用更简洁
export auto ReadFile(std::string filename)
{
    return FileReaderAwaiter(std::move(filename));
}

} // namespace kvdb::core::coro
