/**
 * TCP 服务端示例
 * 
 * 功能：
 * - 监听指定端口，接受多个客户端连接
 * - 接收客户端消息并回显
 * - 支持从终端输入并广播消息给所有客户端
 * - 支持向指定客户端发送消息
 * 
 * 使用方法：
 *   ./tcp_server [ip] [port]
 *   默认：0.0.0.0:8888
 * 
 * 命令：
 *   直接输入文字 - 广播给所有客户端
 *   /send <fd> <消息> - 发送给指定客户端
 *   /list - 列出所有连接的客户端
 *   /quit - 退出服务器
 */

#include "tcp_server.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <sys/select.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>

std::atomic<bool> g_running(true);
TcpServer* g_server = nullptr;

void signal_handler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
    if (g_server) {
        g_server->stop();
    }
}

// 解析 /send 命令：/send <fd> <消息>
bool parse_send_command(const std::string& input, int& fd, std::string& message) {
    if (input.substr(0, 6) != "/send ") {
        return false;
    }
    
    std::istringstream iss(input.substr(6));
    if (!(iss >> fd)) {
        return false;
    }
    
    // 跳过 fd 后的空格，获取剩余的消息
    std::getline(iss >> std::ws, message);
    return !message.empty();
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string ip = "0.0.0.0";
    uint16_t port = 8888;
    
    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "========================================" << std::endl;
    std::cout << "       TCP Server Example" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Binding to: " << ip << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  <message>           - Broadcast to all clients" << std::endl;
    std::cout << "  /send <fd> <msg>    - Send to specific client" << std::endl;
    std::cout << "  /list               - List connected clients" << std::endl;
    std::cout << "  /quit               - Stop server and exit" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 创建TCP服务器
    TcpServer server(ip, port, 4);
    g_server = &server;
    
    // 设置连接回调
    server.set_connection_callback([](int client_fd, const std::string& client_addr) {
        std::cout << "\r[Callback] New connection: fd=" << client_fd 
                  << ", addr=" << client_addr << std::endl;
        std::cout << "> " << std::flush;
    });
    
    // 设置消息回调 - 回显消息
    server.set_message_callback([&server](int client_fd, const std::string& message) {
        std::cout << "\r[Client fd=" << client_fd << "] " << message << std::endl;
        std::cout << "> " << std::flush;
        
        // 回显消息
        std::string response = "[Echo] " + message;
        server.send_to(client_fd, response);
    });
    
    // 设置断开连接回调
    server.set_disconnect_callback([](int client_fd) {
        std::cout << "\r[Callback] Client disconnected: fd=" << client_fd << std::endl;
        std::cout << "> " << std::flush;
    });
    
    // 启动服务器
    if (!server.start()) {
        std::cerr << "Failed to start server!" << std::endl;
        g_server = nullptr;
        return 1;
    }
    
    std::cout << "\nServer is running. Enter messages to broadcast:" << std::endl;
    std::cout << "> " << std::flush;
    
    // 主循环 - 使用 select 非阻塞读取用户输入
    std::string input;
    while (g_running && server.is_running()) {
        // 使用 select 监控 stdin
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms 超时
        
        int ret = select(STDIN_FILENO + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "\nSelect error: " << strerror(errno) << std::endl;
            break;
        }
        
        if (ret == 0) {
            // 超时，继续循环
            continue;
        }
        
        // stdin 有数据可读
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (!std::getline(std::cin, input)) {
                break;
            }
            
            if (input.empty()) {
                std::cout << "> " << std::flush;
                continue;
            }
            
            // 处理命令
            if (input == "/quit") {
                break;
            } else if (input == "/list") {
                // 列出所有客户端
                auto clients = server.get_clients();
                if (clients.empty()) {
                    std::cout << "[Info] No clients connected." << std::endl;
                } else {
                    std::cout << "[Info] Connected clients (" << clients.size() << "):" << std::endl;
                    for (const auto& [fd, addr] : clients) {
                        std::cout << "  fd=" << fd << " -> " << addr << std::endl;
                    }
                }
                std::cout << "> " << std::flush;
            } else if (input.substr(0, 6) == "/send ") {
                // 发送给指定客户端
                int fd;
                std::string message;
                if (parse_send_command(input, fd, message)) {
                    std::string formatted_msg = "[Server] " + message;
                    if (server.send_to(fd, formatted_msg)) {
                        std::cout << "[Sent to fd=" << fd << "] " << message << std::endl;
                    } else {
                        std::cerr << "[Error] Failed to send to fd=" << fd << " (client may not exist)" << std::endl;
                    }
                } else {
                    std::cerr << "[Error] Usage: /send <fd> <message>" << std::endl;
                }
                std::cout << "> " << std::flush;
            } else {
                // 广播消息给所有客户端
                std::string broadcast_msg = "[Server] " + input;
                server.broadcast(broadcast_msg);
                std::cout << "[Broadcast] " << input << std::endl;
                std::cout << "> " << std::flush;
            }
        }
    }
    
    // 停止服务器
    g_server = nullptr;
    server.stop();
    
    std::cout << "Server shutdown complete." << std::endl;
    return 0;
}

