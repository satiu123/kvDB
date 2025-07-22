# tests/CMakeLists.txt

# 找到 GTest 包 (这是现代 CMake 的推荐做法)
find_package(GTest REQUIRED)

add_executable(kvdb_tests
    test_logger.cpp
    # test_database.cpp
)

# 正确的方式: 链接到 CMake 目标，包括 GTest 的目标
target_link_libraries(kvdb_tests 
    PRIVATE 
        kvdb
        GTest::gtest_main # 使用 GTest 提供的导入目标
        GTest::gtest
)

# 添加测试
include(GoogleTest)
gtest_discover_tests(kvdb_tests)