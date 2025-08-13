import kvdb.logging;
import kvdb.logging.log.logger;
import kvdb.logging.sinks.file_sink;

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using kvdb::logging::FileSink;
using kvdb::logging::Logger;
using kvdb::logging::LogLevel;
using namespace std::chrono_literals;

TEST(Logger, CreateAndWrite) {
    std::filesystem::create_directories("./test_logs");
    auto sink_res = FileSink::create("./test_logs");
    ASSERT_TRUE(sink_res.has_value()) << sink_res.error();

    auto& logger = Logger::getInstance();
    logger.addSink(*sink_res);

    kvdb::logging::LOG_INFO()("hello {}", "world");
    kvdb::logging::LOG_ERROR()("oops {}", 42);
    std::this_thread::sleep_for(150ms);

    std::ifstream ifs("./test_logs/kvdb.log");
    ASSERT_TRUE(ifs.good());
    std::string line;
    std::getline(ifs, line);
    // 若异步线程略有延迟，补一次等待
    if (line.empty()) {
        std::this_thread::sleep_for(200ms);
        ifs.clear();
        ifs.seekg(0);
        std::getline(ifs, line);
    }
    EXPECT_FALSE(line.empty());
}
