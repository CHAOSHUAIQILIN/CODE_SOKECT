#include "tcp_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <mutex>

/// @brief 接收缓冲区大小
constexpr int BUFFER_SIZE = 4096;

/// @brief 最大等待连接队列长度
constexpr int MAX_PENDING_CONNECTIONS = 10;

/**
 * @brief 构造函数实现
 * @param ip 服务器绑定的 IP 地址
 * @param port 服务器监听的端口
 * @param thread_pool_size 线程池大小
 */
TcpServer::TcpServer(const std::string& ip, uint16_t port, size_t thread_pool_size)
    : ip_(ip)
    , port_(port)
    , server_fd_(-1)
    , running_(false)
    , thread_pool_(std::make_unique<ThreadPool>(thread_pool_size)) {
}

/**
 * @brief 析构函数实现
 */
TcpServer::~TcpServer() {
    stop();
}

/**
 * @brief 启动服务器
 * @return 启动是否成功
 */
bool TcpServer::start() {
    // 检查是否已在运行
    if (running_) {
        return false;
    }
    
    // 创建 socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[TcpServer] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置地址复用选项，避免 TIME_WAIT 状态导致绑定失败
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[TcpServer] Failed to set socket options: " << strerror(errno) << std::endl;
        close(server_fd_);
        return false;
    }
    
    // 设置服务器地址结构
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    // 转换 IP 地址
    if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[TcpServer] Invalid IP address: " << ip_ << std::endl;
        close(server_fd_);
        return false;
    }
    
    // 绑定地址
    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "[TcpServer] Failed to bind: " << strerror(errno) << std::endl;
        close(server_fd_);
        return false;
    }
    
    // 开始监听
    if (listen(server_fd_, MAX_PENDING_CONNECTIONS) < 0) {
        std::cerr << "[TcpServer] Failed to listen: " << strerror(errno) << std::endl;
        close(server_fd_);
        return false;
    }
    
    running_ = true;
    
    // 启动接受连接的线程
    accept_thread_ = std::thread(&TcpServer::accept_loop, this);
    
    std::cout << "[TcpServer] Server started on " << ip_ << ":" << port_ << std::endl;
    return true;
}

/**
 * @brief 停止服务器
 */
void TcpServer::stop() {
    // 检查是否在运行
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // 关闭服务器 socket，使 accept() 退出阻塞
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    
    // 等待接受线程结束
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& [fd, addr] : clients_) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
        clients_.clear();
    }
    
    std::cout << "[TcpServer] Server stopped" << std::endl;
}

/**
 * @brief 接受客户端连接的循环
 * 
 * @details
 * 在独立线程中持续运行，接受新的客户端连接。
 * 每个新连接会被提交到线程池中处理。
 */
void TcpServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        
        // 接受新连接
        int client_fd = accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "[TcpServer] Accept failed: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // 获取客户端地址字符串
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        std::string client_addr_str = std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));
        
        // 添加到客户端列表
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_[client_fd] = client_addr_str;
        }
        
        std::cout << "[TcpServer] Client connected: " << client_addr_str << " (fd=" << client_fd << ")" << std::endl;
        
        // 触发连接回调
        if (connection_callback_) {
            connection_callback_(client_fd, client_addr_str);
        }
        
        // 提交到线程池处理客户端消息
        thread_pool_->submit([this, client_fd, client_addr_str]() {
            this->handle_client(client_fd, client_addr_str);
        });
    }
}

/**
 * @brief 处理单个客户端的消息
 * @param client_fd 客户端文件描述符
 * @param client_addr 客户端地址字符串
 * 
 * @details
 * 在线程池的工作线程中运行，持续接收客户端消息直到连接断开。
 */
void TcpServer::handle_client(int client_fd, const std::string& client_addr) {
    char buffer[BUFFER_SIZE];
    
    while (running_) {
        // 清空缓冲区
        memset(buffer, 0, sizeof(buffer));
        
        // 接收数据
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                // 客户端正常断开
                std::cout << "[TcpServer] Client disconnected: " << client_addr << std::endl;
            } else if (running_) {
                // 接收错误
                std::cerr << "[TcpServer] Recv error from " << client_addr << ": " << strerror(errno) << std::endl;
            }
            break;
        }
        
        // 构造消息字符串
        std::string message(buffer, bytes_read);
        std::cout << "[TcpServer] Received from " << client_addr << ": " << message << std::endl;
        
        // 触发消息回调
        if (message_callback_) {
            message_callback_(client_fd, message);
        }
    }
    
    // 关闭客户端连接
    close_client(client_fd);
}

/**
 * @brief 关闭指定客户端连接
 * @param client_fd 要关闭的客户端文件描述符
 */
void TcpServer::close_client(int client_fd) {
    // 从客户端列表移除
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(client_fd);
    }
    
    // 关闭 socket
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    
    // 触发断开连接回调
    if (disconnect_callback_) {
        disconnect_callback_(client_fd);
    }
}

/**
 * @brief 向指定客户端发送消息
 * @param client_fd 目标客户端文件描述符
 * @param message 要发送的消息
 * @return 发送是否成功
 */
bool TcpServer::send_to(int client_fd, const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    // 检查客户端是否存在
    if (clients_.find(client_fd) == clients_.end()) {
        return false;
    }
    
    ssize_t bytes_sent = ::send(client_fd, message.c_str(), message.size(), 0);
    return bytes_sent == static_cast<ssize_t>(message.size());
}

/**
 * @brief 向所有客户端广播消息
 * @param message 要广播的消息
 */
void TcpServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    
    for (auto& [fd, addr] : clients_) {
        ::send(fd, message.c_str(), message.size(), 0);
    }
}

/**
 * @brief 设置消息接收回调
 * @param callback 回调函数
 */
void TcpServer::set_message_callback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

/**
 * @brief 设置客户端连接回调
 * @param callback 回调函数
 */
void TcpServer::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
}

/**
 * @brief 设置客户端断开连接回调
 * @param callback 回调函数
 */
void TcpServer::set_disconnect_callback(DisconnectCallback callback) {
    disconnect_callback_ = std::move(callback);
}

/**
 * @brief 获取所有已连接客户端的信息
 * @return 客户端映射表的副本
 */
std::unordered_map<int, std::string> TcpServer::get_clients() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_;
}
