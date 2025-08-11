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
add_executable(test_logger
    test_logger.cpp
)

# Link the test executable against our kvdb library and GoogleTest
# GTest::gtest_main provides the main() function for the test runner
target_link_libraries(test_logger 
    PRIVATE 
        kvdb
        GTest::gtest
        GTest::gtest_main
)

# Discover tests in the executable using gtest_discover_tests
# This allows CTest to run the tests found in the executable
include(GoogleTest)
# gtest_discover_tests(test_logger)