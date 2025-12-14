# udp 模块 - UDP服务端/客户端
add_library(udp STATIC
    src/udp_server.cpp
    src/udp_client.cpp
)

target_include_directories(udp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(udp PUBLIC
    common
)

