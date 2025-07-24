# tests/CMakeLists.txt

# 找到 GTest 包 
find_package(GTest REQUIRED)

add_executable(kvdb_tests
    test_logger.cpp
    test_database.cpp
    test_wal.cpp
)


# 链接到 CMake 目标，使用现代 CMake 的 GTest 目标
target_link_libraries(kvdb_tests 
    PRIVATE 
        kvdb
        GTest::gtest
        GTest::gtest_main
)

# 添加测试
include(GoogleTest)
gtest_discover_tests(kvdb_tests)