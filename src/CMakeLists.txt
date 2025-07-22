# src/CMakeLists.txt

# 1. 创建一个库目标，此时先不指定源文件
add_library(kvdb)

# 2. 自动发现源文件，并将它们收集到一个变量中
file(GLOB_RECURSE KVDB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sinks/*.cpp"
)

# 3. 使用 target_sources() 将发现的源文件添加到目标中
target_sources(kvdb
    PRIVATE
        ${KVDB_SOURCES}
)

# 4. 指定头文件搜索路径 (这部分不变，非常重要)
target_include_directories(kvdb PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
)

# 5. 安装库 (这部分不变)
install(TARGETS kvdb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)