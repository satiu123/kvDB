#include <gtest/gtest.h>

import IOUring;
import File; // 导入我们新的文件模块
import std;
import kvdb.core.coro.task;

class IOUringTest : public ::testing::Test {
  protected:
    void TearDown() override {
        // 在测试结束后尝试删除文件，忽略错误
        try {
            File::remove(test_filename_);
        } catch (const std::exception& e) {
            // 在清理阶段，我们可能不关心文件是否真的被删除
            std::println(stderr, "Cleanup failed but ignored: {}", e.what());
        }
    }

    const std::string test_filename_ = "io_uring_test_file.tmp";
};

Task<bool> test_write_and_read(IOUring& ring, const std::string& filename) {
    File test_file(ring, filename, FileMode::ReadWrite);

    // 写入测试
    const std::string write_data_str = "你好, C++20 模块!";
    std::vector<std::byte> write_buffer(write_data_str.size());
    std::transform(write_data_str.begin(), write_data_str.end(), write_buffer.begin(),
                   [](char c) { return static_cast<std::byte>(c); });

    co_await test_file.write(write_buffer, 0);

    // 读取测试
    std::vector<std::byte> read_buffer(write_buffer.size());
    co_await test_file.read(read_buffer, 0);

    // 验证
    co_return write_buffer == read_buffer;
}

TEST_F(IOUringTest, WriteAndRead) {
    try {
        IOUring ring(8);
        auto task = test_write_and_read(ring, test_filename_);
        task.resume();

        while (!task.done()) {
            ring.wait_for_completion();
        }

        ASSERT_TRUE(task.get());

    } catch (const std::exception& e) {
        FAIL() << "Test failed with exception: " << e.what();
    }
}
