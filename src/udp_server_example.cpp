/**
 * UDP 服务端示例
 * 
 * 功能：
 * - 监听指定端口，接收UDP数据报
 * - 接收消息并回显给发送方
 * - 支持从终端输入并发送消息给客户端
 * 
 * 使用方法：
 *   ./udp_server [ip] [port]
 *   默认：0.0.0.0:9999
 * 
 * 命令：
 *   /send <ip> <port> <消息> - 发送消息给指定地址
 *   /reply <消息> - 回复最后一个发送消息的客户端
 *   /quit - 退出服务器
 */

#include "udp_server.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <sys/select.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <mutex>

std::atomic<bool> g_running(true);
UdpServer* g_server = nullptr;

// 记录最后一个发送消息的客户端
std::string g_last_sender_ip;
uint16_t g_last_sender_port = 0;
std::mutex g_last_sender_mutex;

void signal_handler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
    if (g_server) {
        g_server->stop();
    }
}

// 解析 /send 命令：/send <ip> <port> <消息>
bool parse_send_command(const std::string& input, std::string& ip, uint16_t& port, std::string& message) {
    if (input.substr(0, 6) != "/send ") {
        return false;
    }
    
    std::istringstream iss(input.substr(6));
    int port_int;
    if (!(iss >> ip >> port_int)) {
        return false;
    }
    port = static_cast<uint16_t>(port_int);
    
    // 跳过空格，获取剩余的消息
    std::getline(iss >> std::ws, message);
    return !message.empty();
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string ip = "0.0.0.0";
    uint16_t port = 9999;
    
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
    std::cout << "       UDP Server Example" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Binding to: " << ip << ":" << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  /send <ip> <port> <msg> - Send to specific address" << std::endl;
    std::cout << "  /reply <msg>            - Reply to last sender" << std::endl;
    std::cout << "  /quit                   - Stop server and exit" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 创建UDP服务器
    UdpServer server(ip, port, 4);
    g_server = &server;
    
    // 设置消息回调 - 回显消息并记录发送方
    server.set_message_callback([&server](const std::string& sender_ip, uint16_t sender_port, const std::string& message) {
        // 记录最后一个发送方
        {
            std::lock_guard<std::mutex> lock(g_last_sender_mutex);
            g_last_sender_ip = sender_ip;
            g_last_sender_port = sender_port;
        }
        
        std::cout << "\r[From " << sender_ip << ":" << sender_port << "] " << message << std::endl;
        std::cout << "> " << std::flush;
        
        // 回显消息
        std::string response = "[Echo] " + message;
        server.send_to(sender_ip, sender_port, response);
    });
    
    // 启动服务器
    if (!server.start()) {
        std::cerr << "Failed to start server!" << std::endl;
        g_server = nullptr;
        return 1;
    }
    
    std::cout << "\nServer is running. Use commands to send messages:" << std::endl;
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
            } else if (input.substr(0, 7) == "/reply ") {
                // 回复最后一个发送方
                std::string reply_ip;
                uint16_t reply_port;
                {
                    std::lock_guard<std::mutex> lock(g_last_sender_mutex);
                    reply_ip = g_last_sender_ip;
                    reply_port = g_last_sender_port;
                }
                
                if (reply_ip.empty()) {
                    std::cerr << "[Error] No client to reply to." << std::endl;
                } else {
                    std::string message = input.substr(7);
                    std::string formatted_msg = "[Server] " + message;
                    if (server.send_to(reply_ip, reply_port, formatted_msg)) {
                        std::cout << "[Sent to " << reply_ip << ":" << reply_port << "] " << message << std::endl;
                    } else {
                        std::cerr << "[Error] Failed to send message." << std::endl;
                    }
                }
                std::cout << "> " << std::flush;
            } else if (input.substr(0, 6) == "/send ") {
                // 发送给指定地址
                std::string dest_ip;
                uint16_t dest_port;
                std::string message;
                if (parse_send_command(input, dest_ip, dest_port, message)) {
                    std::string formatted_msg = "[Server] " + message;
                    if (server.send_to(dest_ip, dest_port, formatted_msg)) {
                        std::cout << "[Sent to " << dest_ip << ":" << dest_port << "] " << message << std::endl;
                    } else {
                        std::cerr << "[Error] Failed to send message." << std::endl;
                    }
                } else {
                    std::cerr << "[Error] Usage: /send <ip> <port> <message>" << std::endl;
                }
                std::cout << "> " << std::flush;
            } else {
                std::cerr << "[Error] Unknown command. Use /send, /reply, or /quit." << std::endl;
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

