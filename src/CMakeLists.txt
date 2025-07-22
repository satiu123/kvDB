set(SOURCES
    database.cpp
    log.cpp
)

# 创建库
add_library(k-vDB ${SOURCES})

# 安装库
install(TARGETS k-vDB
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
