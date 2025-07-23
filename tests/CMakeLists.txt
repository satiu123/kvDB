# tests/CMakeLists.txt

# 找到 GTest 包 (这是现代 CMake 的推荐做法)
find_package(GTest REQUIRED)

add_executable(kvdb_tests
    test_logger.cpp
    # test_database.cpp
    test_wal.cpp
)

# 链接到 CMake 目标，包括 GTest 的目标
target_link_libraries(kvdb_tests 
    PRIVATE 
        kvdb
        /usr/lib/libgtest.so
        /usr/lib/libgtest_main.so
        pthread
)

# 添加测试
include(GoogleTest)
gtest_discover_tests(kvdb_tests)