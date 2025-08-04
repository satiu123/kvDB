import std;
import utils.thread_pool;
int main() {
    // 创建一个线程池
    ThreadPool pool(4);

    // 提交任务
    auto future1 = pool.enqueue([] {
        std::cout << "任务 1 正在运行\\n";
        return 42;
    });

    auto future2 = pool.enqueue(
        [](const std::string& msg) {
            std::cout << "任务 2 说: " << msg << "\\n";
            return "完成";
        },
        "你好");

    // 获取结果
    std::cout << "任务 1 结果: " << future1.get() << std::endl;
    std::cout << "任务 2 结果: " << future2.get() << std::endl;

    return 0;
}  // 当 pool 在这里被析构时，它会安全地等待所有任务完成