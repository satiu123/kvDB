cmake_minimum_required(VERSION 3.28)
project(kvDB VERSION 0.1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_library(CXX_LIBRARIES c++)


# 如果使用clang编译器
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Using Clang compiler")
    add_compile_definitions(-D_LIBCPP_REMOVE_TRANSITIVE_INCLUDES)
    add_compile_options(-fdiagnostics-color -pedantic-errors -Wall -Wextra -Wdeprecated
                        -fexperimental-library -stdlib=libc++)
    add_link_options(-fuse-ld=lld -stdlib=libc++ -Wl,--color-diagnostics -fdiagnostics-color
                        -fexperimental-library)

    set(MOD_BASE_DIR /usr/share/libc++/v1)
    set(STD_MODULE_FILE ${MOD_BASE_DIR}/std.cppm)
    set(STD_COMPAT_MODULE_FILE ${MOD_BASE_DIR}/std.compat.cppm)
endif()
# 如果使用GCC编译器
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using GCC compiler")
    add_compile_options(-Wall -Wextra -Wpedantic)

    set(MOD_BASE_DIR /usr/include/c++/${CMAKE_CXX_COMPILER_VERSION}/bits)
    set(STD_MODULE_FILE ${MOD_BASE_DIR}/std.cc)
    set(STD_COMPAT_MODULE_FILE ${MOD_BASE_DIR}/std.compat.cc)
endif()


# add_definitions(-w)
add_library(std-cxx-modules SHARED)
target_sources(std-cxx-modules
  PUBLIC
    FILE_SET moduleStd
    TYPE CXX_MODULES
    BASE_DIRS ${MOD_BASE_DIR}
    FILES
      ${STD_MODULE_FILE}
      ${STD_COMPAT_MODULE_FILE})
target_compile_options(std-cxx-modules
  PRIVATE
    -Wno-reserved-module-identifier
    -Wno-reserved-user-defined-literal)

# 导出编译命令用于clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加子目录
add_subdirectory(src)

# 启用测试
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Add CLI executable
# add_executable(kvdb-cli cli/main.cpp)
# target_link_libraries(kvdb-cli PRIVATE kvdb std-cxx-modules)

# 添加示例
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装规则
install(DIRECTORY include/ DESTINATION include)
