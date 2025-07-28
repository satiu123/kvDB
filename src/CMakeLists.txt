# src/CMakeLists.txt

# 1. 创建一个库目标
add_library(kvdb)

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


target_link_libraries(kvdb
    PRIVATE
        std-cxx-modules
)

install(TARGETS kvdb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET Modules DESTINATION include
)