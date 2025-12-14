/**
 * @file udp_client.cpp
 * @brief UDP 客户端类的实现文件
 * @author Your Name
 * @date 2024
 * @version 1.0.0
 */

#include "udp_client.h"
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
 */
UdpClient::UdpClient()
    : socket_fd_(-1)
    , initialized_(false)
    , receiving_(false) {
}

/**
 * @brief 析构函数实现
 */
UdpClient::~UdpClient() {
    close();
}

/**
 * @brief 初始化客户端
 * @param local_port 本地绑定端口
 * @return 初始化是否成功
 */
bool UdpClient::init(uint16_t local_port) {
    // 检查是否已初始化
    if (initialized_) {
        return false;
    }
    
    // 创建 UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[UdpClient] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 如果指定了本地端口，则绑定
    if (local_port > 0) {
        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = INADDR_ANY;
        local_addr.sin_port = htons(local_port);
        
        if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) < 0) {
            std::cerr << "[UdpClient] Failed to bind local port: " << strerror(errno) << std::endl;
            ::close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        std::cout << "[UdpClient] Bound to local port " << local_port << std::endl;
    }
    
    initialized_ = true;
    std::cout << "[UdpClient] Initialized" << std::endl;
    return true;
}

/**
 * @brief 关闭客户端
 */
void UdpClient::close() {
    // 先停止接收
    stop_receiving();
    
    // 检查是否已初始化
    if (!initialized_) {
        return;
    }
    
    initialized_ = false;
    
    // 关闭 socket
    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_RDWR);
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    
    std::cout << "[UdpClient] Closed" << std::endl;
}

/**
 * @brief 发送消息到指定地址
 * @param ip 目标 IP 地址
 * @param port 目标端口
 * @param message 要发送的消息
 * @return 发送是否成功
 */
bool UdpClient::send_to(const std::string& ip, uint16_t port, const std::string& message) {
    // 检查初始化状态
    if (!initialized_) {
        std::cerr << "[UdpClient] Not initialized" << std::endl;
        return false;
    }
    
    // 设置目标地址
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    // 转换 IP 地址
    if (inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr) <= 0) {
        std::cerr << "[UdpClient] Invalid destination IP: " << ip << std::endl;
        return false;
    }
    
    // 加锁发送
    std::lock_guard<std::mutex> lock(send_mutex_);
    ssize_t bytes_sent = sendto(socket_fd_, message.c_str(), message.size(), 0,
                                 reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
    
    if (bytes_sent < 0) {
        std::cerr << "[UdpClient] Sendto failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "[UdpClient] Sent to " << ip << ":" << port << " - " << message << std::endl;
    return bytes_sent == static_cast<ssize_t>(message.size());
}

/**
 * @brief 开始接收消息
 */
void UdpClient::start_receiving() {
    // 检查状态
    if (!initialized_ || receiving_) {
        return;
    }
    
    receiving_ = true;
    receive_thread_ = std::thread(&UdpClient::receive_loop, this);
    std::cout << "[UdpClient] Started receiving" << std::endl;
}

/**
 * @brief 停止接收消息
 */
void UdpClient::stop_receiving() {
    // 检查状态
    if (!receiving_) {
        return;
    }
    
    receiving_ = false;
    
    // 等待接收线程结束
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    
    std::cout << "[UdpClient] Stopped receiving" << std::endl;
}

/**
 * @brief 消息接收循环
 * 
 * @details
 * 在独立线程中持续运行，接收来自任意地址的 UDP 数据报。
 * 使用接收超时机制，以便能够响应 stop_receiving() 调用。
 */
void UdpClient::receive_loop() {
    char buffer[BUFFER_SIZE];
    
    // 设置接收超时，以便能够检查 receiving_ 标志
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    while (receiving_) {
        sockaddr_in sender_addr{};
        socklen_t addr_len = sizeof(sender_addr);
        
        // 清空缓冲区
        memset(buffer, 0, sizeof(buffer));
        
        // 接收数据
        ssize_t bytes_read = recvfrom(socket_fd_, buffer, sizeof(buffer) - 1, 0,
                                       reinterpret_cast<sockaddr*>(&sender_addr), &addr_len);
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 接收超时，继续循环检查 receiving_ 标志
                continue;
            }
            if (receiving_) {
                std::cerr << "[UdpClient] Recvfrom failed: " << strerror(errno) << std::endl;
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
        
        std::cout << "[UdpClient] Received from " << sender_ip << ":" << sender_port 
                  << " - " << message << std::endl;
        
        // 触发消息回调
        if (message_callback_) {
            message_callback_(sender_ip, sender_port, message);
        }
    }
}

/**
 * @brief 设置消息接收回调
 * @param callback 回调函数
 */
void UdpClient::set_message_callback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}
