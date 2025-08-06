# src/CMakeLists.txt

# 1. 创建一个库目标
add_library(kvdb SHARED)

# 2. 收集所有模块接口文件 (.cppm)
file(GLOB_RECURSE KVDB_MODULE_INTERFACES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cppm"
)

# 3. 收集所有源文件 (.cpp)
file(GLOB_RECURSE KVDB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# 4. 添加模块接口文件和源文件到目标
target_sources(kvdb
    PUBLIC
        FILE_SET CXX_MODULES FILES
            ${KVDB_MODULE_INTERFACES}
    PRIVATE
            ${KVDB_SOURCES}
)

# 5. 设置模块编译属性
set_target_properties(kvdb PROPERTIES
    CXX_SCAN_FOR_MODULES ON
)

# 查找 pkg-config 工具
find_package(PkgConfig REQUIRED)

# 使用 pkg-config 查找 liburing
# 这会创建一个导入的目标 PkgConfig::liburing
pkg_check_modules(LIBURING REQUIRED IMPORTED_TARGET liburing)
target_link_libraries(kvdb
    PRIVATE
        std-cxx-modules
        ${LIBURING_LIBRARIES}
)

install(TARGETS kvdb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET Modules DESTINATION include
)

# Added io_uring module