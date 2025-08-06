#include <gtest/gtest.h>

import IOUring;
import File; // 导入我们新的文件模块
import std;

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

TEST_F(IOUringTest, WriteAndRead) {
    try {
        // 1. 使用File模块创建和管理文件，实现RAII
        File test_file(test_filename_, FileMode::ReadWrite);
        IOUring ring(8);

        // 2. 写入测试
        const std::string write_data_str = "你好, C++20 模块!";
        std::vector<std::byte> write_buffer(write_data_str.size());
        std::transform(write_data_str.begin(), write_data_str.end(), write_buffer.begin(),
                       [](char c) { return static_cast<std::byte>(c); });

        ring.submit_write(test_file.get_fd(), write_buffer, 0, 1);
        CompletionResult write_result = ring.wait_for_completion();

        ASSERT_EQ(write_result.user_data, 1);
        ASSERT_GE(write_result.result, 0);
        ASSERT_EQ(static_cast<std::size_t>(write_result.result), write_buffer.size());

        // 3. 读取测试
        std::vector<std::byte> read_buffer(write_buffer.size());
        ring.submit_read(test_file.get_fd(), read_buffer, 0, 2);
        CompletionResult read_result = ring.wait_for_completion();

        ASSERT_EQ(read_result.user_data, 2);
        ASSERT_GE(read_result.result, 0);
        ASSERT_EQ(static_cast<std::size_t>(read_result.result), read_buffer.size());

        // 4. 验证
        ASSERT_EQ(write_buffer, read_buffer);

    } catch (const std::exception& e) {
        FAIL() << "Test failed with exception: " << e.what();
    }
}
