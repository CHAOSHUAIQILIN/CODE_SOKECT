/**
 * @file udp_server.cpp
 * @brief UDP 服务器类的实现文件
 * @author Your Name
 * @date 2024
 * @version 1.0.0
 */

#include "udp_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

/// @brief 接收缓冲区大小（UDP 最大数据报大小）
constexpr int BUFFER_SIZE = 65535;

/**
 * @brief 构造函数实现
 * @param ip 服务器绑定的 IP 地址
 * @param port 服务器监听的端口
 * @param thread_pool_size 线程池大小
 */
UdpServer::UdpServer(const std::string& ip, uint16_t port, size_t thread_pool_size)
    : ip_(ip)
    , port_(port)
    , socket_fd_(-1)
    , running_(false)
    , thread_pool_(std::make_unique<ThreadPool>(thread_pool_size)) {
}

/**
 * @brief 析构函数实现
 */
UdpServer::~UdpServer() {
    stop();
}

/**
 * @brief 启动服务器
 * @return 启动是否成功
 */
bool UdpServer::start() {
    // 检查是否已在运行
    if (running_) {
        return false;
    }
    
    // 创建 UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[UdpServer] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置地址复用选项
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[UdpServer] Failed to set socket options: " << strerror(errno) << std::endl;
        ::close(socket_fd_);
        return false;
    }
    
    // 设置服务器地址结构
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    // 转换 IP 地址
    if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[UdpServer] Invalid IP address: " << ip_ << std::endl;
        ::close(socket_fd_);
        return false;
    }
    
    // 绑定地址
    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "[UdpServer] Failed to bind: " << strerror(errno) << std::endl;
        ::close(socket_fd_);
        return false;
    }
    
    running_ = true;
    
    // 启动接收线程
    receive_thread_ = std::thread(&UdpServer::receive_loop, this);
    
    std::cout << "[UdpServer] Server started on " << ip_ << ":" << port_ << std::endl;
    return true;
}

/**
 * @brief 停止服务器
 */
void UdpServer::stop() {
    // 检查是否在运行
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // 关闭 socket，使 recvfrom() 退出阻塞
    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_RDWR);
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    
    // 等待接收线程结束
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    
    std::cout << "[UdpServer] Server stopped" << std::endl;
}

/**
 * @brief 消息接收循环
 * 
 * @details
 * 在独立线程中持续运行，接收 UDP 数据报。
 * 每个接收到的消息会被提交到线程池中处理。
 */
void UdpServer::receive_loop() {
    char buffer[BUFFER_SIZE];
    
    while (running_) {
        sockaddr_in sender_addr{};
        socklen_t addr_len = sizeof(sender_addr);
        
        // 清空缓冲区
        memset(buffer, 0, sizeof(buffer));
        
        // 接收数据
        ssize_t bytes_read = recvfrom(socket_fd_, buffer, sizeof(buffer) - 1, 0,
                                       reinterpret_cast<sockaddr*>(&sender_addr), &addr_len);
        
        if (bytes_read < 0) {
            if (running_) {
                std::cerr << "[UdpServer] Recvfrom failed: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // 获取发送方地址
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, ip_str, sizeof(ip_str));
        std::string sender_ip(ip_str);
        uint16_t sender_port = ntohs(sender_addr.sin_port);
        
        // 构造消息字符串
        std::string message(buffer, bytes_read);
        
        std::cout << "[UdpServer] Received from " << sender_ip << ":" << sender_port 
                  << " - " << message << std::endl;
        
        // 提交到线程池处理
        thread_pool_->submit([this, sender_ip, sender_port, message]() {
            this->process_message(sender_ip, sender_port, message);
        });
    }
}

/**
 * @brief 处理接收到的消息
 * @param sender_ip 发送方 IP 地址
 * @param sender_port 发送方端口
 * @param message 接收到的消息
 * 
 * @details 在线程池的工作线程中运行
 */
void UdpServer::process_message(const std::string& sender_ip, uint16_t sender_port, const std::string& message) {
    // 触发消息回调
    if (message_callback_) {
        message_callback_(sender_ip, sender_port, message);
    }
}

/**
 * @brief 发送消息到指定地址
 * @param ip 目标 IP 地址
 * @param port 目标端口
 * @param message 要发送的消息
 * @return 发送是否成功
 */
bool UdpServer::send_to(const std::string& ip, uint16_t port, const std::string& message) {
    // 检查运行状态
    if (!running_) {
        return false;
    }
    
    // 设置目标地址
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    // 转换 IP 地址
    if (inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr) <= 0) {
        std::cerr << "[UdpServer] Invalid destination IP: " << ip << std::endl;
        return false;
    }
    
    // 发送数据
    ssize_t bytes_sent = sendto(socket_fd_, message.c_str(), message.size(), 0,
                                 reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    
    if (bytes_sent < 0) {
        std::cerr << "[UdpServer] Sendto failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    return bytes_sent == static_cast<ssize_t>(message.size());
}

/**
 * @brief 设置消息接收回调
 * @param callback 回调函数
 */
void UdpServer::set_message_callback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}
