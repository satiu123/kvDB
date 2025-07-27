# src/CMakeLists.txt

# 1. 创建一个库目标
add_library(kvdb)

# 2. 收集模块接口文件
file(GLOB_RECURSE KVDB_MODULE_INTERFACES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/wal/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/manager/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/snapshot/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/sinks/*.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/log/*.cppm"
)

# 3. 收集源文件
file(GLOB_RECURSE KVDB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/wal/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/manager/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/snapshot/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/sinks/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/log/*.cpp"
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

# 6. 指定头文件搜索路径
target_include_directories(kvdb PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
)

# 5. 安装库 (这部分不变)
install(TARGETS kvdb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET Modules DESTINATION include
)