#include "kvdb/log.h"
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

// 测试日志系统的使用
TEST(LoggerTest, BasicLogFunctionality) {
    // 常量定义
    constexpr int DEBUG_VALUE = 42;
    constexpr int WARNING_THRESHOLD = 85;
    constexpr int ERROR_CODE = 404;
    constexpr int THREAD_COUNT = 5;
    constexpr int TASKS_PER_THREAD = 3;
    constexpr int SLEEP_DURATION_MS = 100;

    // 获取日志实例
    auto& logger = kvdb::Logger::getInstance();
    
    // 添加控制台接收器
    logger.addSink(std::make_shared<kvdb::ConsoleSink>());
    
    // 添加文件接收器
    try {
        logger.addSink(std::make_shared<kvdb::FileSink>("test_log.log"));
        std::cout << "成功添加文件日志接收器\n";
    } catch (const std::exception& e) {
        std::cerr << "添加文件日志接收器失败: " << e.what() << "\n";
    }
    
    // 设置日志级别
    logger.setLevel(kvdb::LogLevel::DEBUG);
    
    // 使用便捷函数记录不同级别的日志
    LOG_DEBUG("这是一条调试日志，当前值: {}", DEBUG_VALUE);
    LOG_INFO("这是一条信息日志，操作: {}", "初始化系统");
    LOG_WARNING("这是一条警告日志，资源使用率: {}%", WARNING_THRESHOLD);
    LOG_ERROR("这是一条错误日志，错误码: {}", ERROR_CODE);
    
    // 测试多线程日志记录
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);  // 预分配容量
    
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([i,SLEEP_DURATION_MS]() {
            for (int j = 0; j < TASKS_PER_THREAD; ++j) {
                LOG_INFO("线程 {} 正在执行任务 {}", i, j);
                std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_DURATION_MS));
            }
        });
    }
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 移除所有日志接收器
    logger.removeAllSinks();
    std::cout << "日志测试完成\n";
}
