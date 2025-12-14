/**
 * TCP 客户端示例
 * 
 * 功能：
 * - 连接到TCP服务器
 * - 从标准输入读取消息并发送
 * - 接收并显示服务器响应
 * 
 * 使用方法：
 *   ./tcp_client [server_ip] [server_port]
 *   默认：127.0.0.1:8888
 */

#include "tcp_client.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <sys/select.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

std::atomic<bool> g_running(true);
TcpClient* g_client = nullptr;

void signal_handler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
    // 断开客户端连接，让主循环退出
    if (g_client) {
        g_client->disconnect();
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 8888;
    
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = static_cast<uint16_t>(std::stoi(argv[2]));
    }
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "========================================" << std::endl;
    std::cout << "       TCP Client Example" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Connecting to: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 创建TCP客户端
    TcpClient client;
    g_client = &client;  // 保存指针供信号处理器使用
    
    // 设置消息回调
    client.set_message_callback([](const std::string& message) {
        // 清除当前行并显示服务器消息，然后重新显示提示符
        std::cout << "\r[Server] " << message << std::endl;
        std::cout << "> " << std::flush;
    });
    
    // 设置连接状态回调
    client.set_connection_callback([](bool connected) {
        if (connected) {
            std::cout << "[Status] Connected to server" << std::endl;
        } else {
            std::cout << "[Status] Disconnected from server" << std::endl;
            g_running = false;  // 断开连接时也退出
        }
    });
    
    // 连接服务器
    if (!client.connect(server_ip, server_port)) {
        std::cerr << "Failed to connect to server!" << std::endl;
        g_client = nullptr;
        return 1;
    }
    
    std::cout << "\nEnter messages to send (empty line or Ctrl+C to quit):" << std::endl;
    
    // 主循环 - 使用 select 非阻塞读取用户输入
    std::string input;
    std::cout << "> " << std::flush;
    
    while (g_running && client.is_connected()) {
        // 使用 select 监控 stdin，设置超时以便定期检查退出标志
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms 超时
        
        int ret = select(STDIN_FILENO + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ret < 0) {
            if (errno == EINTR) {
                // 被信号中断，继续检查退出标志
                continue;
            }
            std::cerr << "\nSelect error: " << strerror(errno) << std::endl;
            break;
        }
        
        if (ret == 0) {
            // 超时，继续循环检查退出标志（不做任何输出操作）
            continue;
        }
        
        // stdin 有数据可读
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (!std::getline(std::cin, input)) {
                break;
            }
            
            if (input.empty()) {
                break;
            }
            
            if (!client.send(input)) {
                std::cerr << "Failed to send message!" << std::endl;
            }
            
            // 发送成功后显示新的提示符
            std::cout << "> " << std::flush;
        }
    }
    
    // 断开连接
    g_client = nullptr;
    client.disconnect();
    
    std::cout << "Client shutdown complete." << std::endl;
    return 0;
}

