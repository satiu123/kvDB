cmake_minimum_required(VERSION 3.28)
project(kvDB VERSION 0.1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -stdlib=libc++")

# 启用模块支持
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules -fmodules-cache-path=${CMAKE_BINARY_DIR}/module_cache")

set(STD_PCM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/stdm/std.pcm")
set(STD_COMPAT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/stdm/std.compat.pcm")
# 添加模块文件标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodule-file=std=${STD_PCM_PATH} -fmodule-file=std.compat=${STD_COMPAT_PATH}")

# 包含目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 导出编译命令用于clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加子目录
add_subdirectory(src)

# 启用测试
# option(BUILD_TESTS "Build tests" ON)
# if(BUILD_TESTS)
#     enable_testing()
#     add_subdirectory(tests)
# endif()

# Add CLI executable
add_executable(kvdb-cli cli/main.cpp)
target_link_libraries(kvdb-cli PRIVATE kvdb)
# 添加示例
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装规则
install(DIRECTORY include/ DESTINATION include)
