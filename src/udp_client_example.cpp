/**
 * UDP 客户端示例
 * 
 * 功能：
 * - 发送UDP数据报到服务器
 * - 接收并显示服务器响应
 * 
 * 使用方法：
 *   ./udp_client [server_ip] [server_port]
 *   默认：127.0.0.1:9999
 */

#include "udp_client.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <sys/select.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

std::atomic<bool> g_running(true);
UdpClient* g_client = nullptr;

void signal_handler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
    // 关闭客户端，让主循环退出
    if (g_client) {
        g_client->close();
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string server_ip = "127.0.0.1";
    uint16_t server_port = 9999;
    
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
    std::cout << "       UDP Client Example" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Target server: " << server_ip << ":" << server_port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 创建UDP客户端
    UdpClient client;
    g_client = &client;  // 保存指针供信号处理器使用
    
    // 设置消息回调
    client.set_message_callback([](const std::string& sender_ip, uint16_t sender_port, const std::string& message) {
        std::cout << "\r[From " << sender_ip << ":" << sender_port << "] " << message << std::endl;
        std::cout << "> " << std::flush;
    });
    
    // 初始化客户端（绑定一个随机本地端口用于接收响应）
    if (!client.init(0)) {
        std::cerr << "Failed to initialize client!" << std::endl;
        g_client = nullptr;
        return 1;
    }
    
    // 开始接收消息
    client.start_receiving();
    
    std::cout << "\nEnter messages to send (empty line or Ctrl+C to quit):" << std::endl;
    
    // 主循环 - 使用 select 非阻塞读取用户输入
    std::string input;
    std::cout << "> " << std::flush;
    
    while (g_running) {
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
            // 超时，继续循环检查退出标志
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
            
            if (!client.send_to(server_ip, server_port, input)) {
                std::cerr << "Failed to send message!" << std::endl;
            }
            
            // 发送成功后显示新的提示符
            std::cout << "> " << std::flush;
        }
    }
    
    // 关闭客户端
    g_client = nullptr;
    client.close();
    
    std::cout << "Client shutdown complete." << std::endl;
    return 0;
}

