#include "tcp_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

/// @brief 接收缓冲区大小
constexpr int BUFFER_SIZE = 4096;

/**
 * @brief 构造函数实现
 */
TcpClient::TcpClient() : socket_fd_(-1), connected_(false) {}

/**
 * @brief 析构函数实现
 */
TcpClient::~TcpClient() {
    disconnect();
}

/**
 * @brief 连接到服务器
 * @param ip 服务器 IP 地址
 * @param port 服务器端口
 * @return 连接是否成功
 */
bool TcpClient::connect(const std::string& ip, uint16_t port) {
    // 检查是否已连接
    if (connected_) {
        return false;
    }

    // 创建 socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "[TcpClient] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    // 设置服务器地址结构
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    // 转换 IP 地址
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[TcpClient] Invalid IP address: " << ip << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // 发起连接
    if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "[TcpClient] Failed to connect: " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "[TcpClient] Connected to " << ip << ":" << port << std::endl;

    // 触发连接回调
    if (connection_callback_) {
        connection_callback_(true);
    }

    // 启动接收线程
    receive_thread_ = std::thread(&TcpClient::receive_loop, this);

    return true;
}

/**
 * @brief 断开与服务器的连接
 */
void TcpClient::disconnect() {
    // 检查是否已连接
    if (!connected_) {
        return;
    }

    connected_ = false;

    // 关闭 socket
    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_RDWR);
        close(socket_fd_);
        socket_fd_ = -1;
    }

    // 等待接收线程结束
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    std::cout << "[TcpClient] Disconnected" << std::endl;

    // 触发连接回调
    if (connection_callback_) {
        connection_callback_(false);
    }
}

/**
 * @brief 发送消息到服务器
 * @param message 要发送的消息
 * @return 发送是否成功
 */
bool TcpClient::send(const std::string& message) {
    // 检查连接状态
    if (!connected_) {
        return false;
    }

    // 加锁保证线程安全
    std::lock_guard<std::mutex> lock(send_mutex_);
    ssize_t bytes_sent = ::send(socket_fd_, message.c_str(), message.size(), 0);

    if (bytes_sent < 0) {
        std::cerr << "[TcpClient] Send failed: " << strerror(errno) << std::endl;
        return false;
    }

    return bytes_sent == static_cast<ssize_t>(message.size());
}

/**
 * @brief 消息接收循环
 *
 * @details
 * 在独立线程中持续运行，接收服务器发送的消息。
 * 当连接断开或发生错误时退出循环。
 */
void TcpClient::receive_loop() {
#if 0
    char buffer[BUFFER_SIZE];
    
    while (connected_) {
        // 清空缓冲区
        memset(buffer, 0, sizeof(buffer));
        
        // 接收数据
        ssize_t bytes_read = recv(socket_fd_, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                // 服务器正常关闭连接
                std::cout << "[TcpClient] Server closed connection" << std::endl;
            } else if (connected_) {
                // 接收错误
                std::cerr << "[TcpClient] Recv error: " << strerror(errno) << std::endl;
            }
            break;
        }
        
        // 构造消息字符串
        std::string message(buffer, bytes_read);
        std::cout << "[TcpClient] Received: " << message << std::endl;
        
        // 触发消息回调
        if (message_callback_) {
            message_callback_(message);
        }
    }

#else  // 使用 select 实现
    while (connected_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_, &read_fds);

        struct timeval timeout;
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        int ret = select(socket_fd_ + 1, &read_fds, NULL, NULL, &timeout);
        if (ret < 0) {
            std::cerr << "[TcpClient] Select failed: " << strerror(errno) << std::endl;
            return;
        }

if (FD_ISSET(socket_fd_, &read_fds)) {
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytes_read = recv(socket_fd_, buffer, sizeof(buffer) - 1, 0);

            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    std::cout << "[TcpClient] Server closed connection" << std::endl;
                } else {
                    std::cerr << "[TcpClient] Recv error: " << strerror(errno) << std::endl;
                }
                return;
            }

            std::string message(buffer, bytes_read);
            std::cout << "[TcpClient] Received: " << message << std::endl;

            if (message_callback_) {
                message_callback_(message);
            }
        }
    }

#endif
    // 如果是服务器端断开连接，更新本地状态
    if (connected_) {
        connected_ = false;
        if (connection_callback_) {
            connection_callback_(false);
        }
    }
}

/**
 * @brief 设置消息接收回调
 * @param callback 回调函数
 */
void TcpClient::set_message_callback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

/**
 * @brief 设置连接状态变化回调
 * @param callback 回调函数
 */
void TcpClient::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
}
