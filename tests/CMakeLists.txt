# tests/CMakeLists.txt

# Enable testing
enable_testing()

# Include FetchContent module to fetch GoogleTest
include(FetchContent)

# Declare the dependency on GoogleTest
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

# Make GoogleTest available to the project
# This will download the source and add gtest, gtest_main, etc. targets
FetchContent_MakeAvailable(googletest)

# --- Test Executable Definition ---

# Define the test executable for the logger
# Define a single test executable containing all test sources
add_executable(test_kvdb
    test_logger.cpp
    test_binary.cpp
    test_manifest.cpp
    test_bloom_filter.cpp
    test_wal_record.cpp
    test_io.cpp
    test_async_wal.cpp
    test_async_database.cpp
    test_sstable.cpp
    test_async_manifest.cpp
    test_async_database_stress.cpp
)

# Link the test executable against our kvdb library and GoogleTest
target_link_libraries(test_kvdb 
    PRIVATE 
        kvdb
        GTest::gtest
        GTest::gtest_main
)

# Discover tests in the executable using gtest_discover_tests
include(GoogleTest)
gtest_discover_tests(test_kvdb)