# ============================================================================
# src/ - 主程序/示例程序
# ============================================================================

# ============================================================================
# 创建可执行文件
# 语法: add_executable(<可执行文件名> <源文件...>)
# 生成的可执行文件会输出到 CMAKE_RUNTIME_OUTPUT_DIRECTORY (build/bin/)
# ============================================================================

# TCP 示例 - 服务端
add_executable(tcp_server tcp_server_example.cpp)
# 链接 tcp 库，使用 PRIVATE 因为示例程序是最终目标，不会被其他目标依赖
# PRIVATE 链接意味着:
# - tcp_server 可以使用 tcp 库的功能
# - 但 tcp 的依赖不会传递（这里没关系，因为 tcp_server 是可执行文件）
target_link_libraries(tcp_server PRIVATE tcp)

# TCP 示例 - 客户端
add_executable(tcp_client tcp_client_example.cpp)
target_link_libraries(tcp_client PRIVATE tcp)

# UDP 示例 - 服务端
add_executable(udp_server udp_server_example.cpp)
target_link_libraries(udp_server PRIVATE udp)

# UDP 示例 - 客户端
add_executable(udp_client udp_client_example.cpp)
target_link_libraries(udp_client PRIVATE udp)

