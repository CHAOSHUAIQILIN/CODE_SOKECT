/**
 * @file tcp_server.h
 * @brief TCP 服务器类的头文件
 * @author CSQL
 * @date 2025-12-14
 * 
 * @details
 * 提供多客户端 TCP 服务器功能，支持：
 * - 监听指定端口并接受客户端连接
 * - 使用线程池处理多个客户端
 * - 向单个客户端或所有客户端发送消息
 * - 通过回调处理连接、断开和消息事件
 * 
 * @note 该类不可拷贝
 * 
 * @example
 * @code
 * TcpServer server("0.0.0.0", 8080);
 * server.set_message_callback([&](int fd, const std::string& msg) {
 *     server.send_to(fd, "Echo: " + msg);
 * });
 * server.start();
 * @endcode
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <string>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <memory>
#include "thread_pool.h"

/**
 * @class TcpServer
 * @brief TCP 服务器类，支持多客户端并发连接
 * 
 * @details
 * 该类实现了一个基于线程池的 TCP 服务器：
 * - 主线程负责接受新连接
 * - 线程池中的工作线程负责处理客户端消息
 * - 使用回调机制通知上层应用各种事件
 */
class TcpServer {
public:
    /**
     * @brief 消息接收回调函数类型
     * @param client_fd 发送消息的客户端文件描述符
     * @param message 接收到的消息内容
     */
    using MessageCallback = std::function<void(int client_fd, const std::string& message)>;
    
    /**
     * @brief 客户端连接回调函数类型
     * @param client_fd 新连接的客户端文件描述符
     * @param client_addr 客户端地址（格式：IP:Port）
     */
    using ConnectionCallback = std::function<void(int client_fd, const std::string& client_addr)>;
    
    /**
     * @brief 客户端断开连接回调函数类型
     * @param client_fd 断开连接的客户端文件描述符
     */
    using DisconnectCallback = std::function<void(int client_fd)>;
    
    /**
     * @brief 构造函数
     * @param ip 服务器绑定的 IP 地址（如 "0.0.0.0" 表示所有接口）
     * @param port 服务器监听的端口号
     * @param thread_pool_size 线程池大小，默认为 4
     */
    TcpServer(const std::string& ip, uint16_t port, size_t thread_pool_size = 4);
    
    /**
     * @brief 析构函数
     * @details 自动停止服务器并释放资源
     */
    ~TcpServer();
    
    /// @brief 禁止拷贝构造
    TcpServer(const TcpServer&) = delete;
    /// @brief 禁止拷贝赋值
    TcpServer& operator=(const TcpServer&) = delete;
    
    /**
     * @brief 启动服务器
     * @return true 启动成功，false 启动失败
     * 
     * @details
     * 启动流程：
     * 1. 创建 socket
     * 2. 绑定地址和端口
     * 3. 开始监听
     * 4. 启动接受连接的线程
     */
    bool start();
    
    /**
     * @brief 停止服务器
     * 
     * @details
     * 停止流程：
     * 1. 关闭服务器 socket
     * 2. 等待接受线程结束
     * 3. 关闭所有客户端连接
     */
    void stop();
    
    /**
     * @brief 向指定客户端发送消息
     * @param client_fd 目标客户端的文件描述符
     * @param message 要发送的消息内容
     * @return true 发送成功，false 发送失败或客户端不存在
     * 
     * @note 该函数是线程安全的
     */
    bool send_to(int client_fd, const std::string& message);
    
    /**
     * @brief 向所有已连接的客户端广播消息
     * @param message 要广播的消息内容
     * 
     * @note 该函数是线程安全的
     */
    void broadcast(const std::string& message);
    
    /**
     * @brief 设置消息接收回调
     * @param callback 接收到客户端消息时调用的回调函数
     */
    void set_message_callback(MessageCallback callback);
    
    /**
     * @brief 设置客户端连接回调
     * @param callback 有新客户端连接时调用的回调函数
     */
    void set_connection_callback(ConnectionCallback callback);
    
    /**
     * @brief 设置客户端断开连接回调
     * @param callback 客户端断开连接时调用的回调函数
     */
    void set_disconnect_callback(DisconnectCallback callback);
    
    /**
     * @brief 获取服务器运行状态
     * @return true 正在运行，false 已停止
     */
    bool is_running() const { return running_; }
    
    /**
     * @brief 获取所有已连接客户端的信息
     * @return 客户端映射表的副本（fd -> 地址字符串）
     */
    std::unordered_map<int, std::string> get_clients() const;
    
private:
    /**
     * @brief 接受客户端连接的循环（在独立线程中运行）
     */
    void accept_loop();
    
    /**
     * @brief 处理单个客户端的消息（在线程池中运行）
     * @param client_fd 客户端文件描述符
     * @param client_addr 客户端地址字符串
     */
    void handle_client(int client_fd, const std::string& client_addr);
    
    /**
     * @brief 关闭指定客户端连接
     * @param client_fd 要关闭的客户端文件描述符
     */
    void close_client(int client_fd);
    
    std::string ip_;                                    // 服务器绑定的 IP 地址
    uint16_t port_;                                     // 服务器监听的端口
    int server_fd_;                                     // 服务器 socket 文件描述符
    std::atomic<bool> running_;                         // 服务器运行状态标志
    
    std::unique_ptr<ThreadPool> thread_pool_;           // 线程池指针
    std::thread accept_thread_;                         // 接受连接的线程
    
    std::unordered_map<int, std::string> clients_;      // 客户端映射表（fd -> 地址）
    mutable std::mutex clients_mutex_;                  // 客户端列表互斥锁
    
    MessageCallback message_callback_;                  // 消息接收回调
    ConnectionCallback connection_callback_;            // 连接回调
    DisconnectCallback disconnect_callback_;            // 断开连接回调
};

#endif // TCP_SERVER_H
