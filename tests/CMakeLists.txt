# 引入 FetchContent 模块
include(FetchContent)

# 声明 GoogleTest 的源码来源
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

# 设置 GTest 选项，使其可以被其他目标安全地使用
# ON 表示构建 gtest 和 gtest_main 为静态库
# OFF 表示不安装 GTest，因为它只是我们项目的一部分
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 您的测试可执行文件定义（无需改动）
add_executable(kvdb_tests
    test_logger.cpp
    test_database.cpp
    test_wal.cpp
    test_snapshot.cpp
    test_memtable.cpp
    test_compaction.cpp
)

# 链接到 CMake 目标，GTest 的目标名称由 FetchContent 自动创建
# 注意：GTest 1.11.0 版本后，目标名称从 GTest::GTest 变成了 GTest::gtest
target_link_libraries(kvdb_tests 
    PRIVATE 
        kvdb
        GTest::gtest      # 使用 FetchContent 创建的目标
        GTest::gtest_main # 使用 FetchContent 创建的目标
)

# 添加测试（无需改动）
include(GoogleTest)
gtest_discover_tests(kvdb_tests)