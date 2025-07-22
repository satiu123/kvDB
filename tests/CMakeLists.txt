# Google Test查找模块
find_package(GTest QUIET)
if(NOT GTEST_FOUND)
    # 如果没找到，可以从源代码构建
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.11.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

set(TEST_SOURCES
    # test_database.cpp
    test_logger.cpp
)

add_executable(kvdb_tests ${TEST_SOURCES})
target_link_libraries(kvdb_tests k-vDB GTest::gtest_main)

add_test(NAME kvdb_tests COMMAND kvdb_tests)
