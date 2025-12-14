# ============================================================================
# tcp 模块 - TCP 服务端/客户端
# ============================================================================

# 创建静态库 tcp，包含服务端和客户端实现
add_library(tcp STATIC
    src/tcp_server.cpp
    src/tcp_client.cpp
)

# 设置头文件路径为 PUBLIC
# 效果: 链接 tcp 库的目标会自动获得 tcp/include 的头文件搜索路径
target_include_directories(tcp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接 common 模块
# 因为 common 的 include 和 pthread 都是 PUBLIC，所以:
# - tcp 可以 #include "thread_pool.h"
# - tcp 自动链接 pthread
# - 链接 tcp 的目标也能使用 common 的头文件
target_link_libraries(tcp PUBLIC
    common
)

