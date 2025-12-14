/**
 * @file udp_server.h
 * @brief UDP 服务器类的头文件
 * @author Your Name
 * @date 2024
 * @version 1.0.0
 * 
 * @details
 * 提供 UDP 服务器功能，支持：
 * - 绑定指定地址和端口接收数据报
 * - 使用线程池处理接收到的消息
 * - 向任意地址发送响应
 * - 通过回调处理接收到的消息
 * 
 * @note 该类不可拷贝
 * 
 * @example
 * @code
 * UdpServer server("0.0.0.0", 8080);
 * server.set_message_callback([&](const std::string& ip, uint16_t port, const std::string& msg) {
 *     server.send_to(ip, port, "Echo: " + msg);
 * });
 * server.start();
 * @endcode
 */

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <memory>
#include "thread_pool.h"

/**
 * @class UdpServer
 * @brief UDP 服务器类，用于接收和响应 UDP 数据报
 * 
 * @details
 * 该类实现了一个基于线程池的 UDP 服务器：
 * - 主接收线程负责接收数据报
 * - 线程池中的工作线程负责处理消息
 * - 使用回调机制通知上层应用
 */
class UdpServer {
public:
    /**
     * @brief 消息接收回调函数类型
     * @param sender_ip 发送方 IP 地址
     * @param sender_port 发送方端口号
     * @param message 接收到的消息内容
     */
    using MessageCallback = std::function<void(const std::string& sender_ip, uint16_t sender_port, const std::string& message)>;
    
    /**
     * @brief 构造函数
     * @param ip 服务器绑定的 IP 地址（如 "0.0.0.0" 表示所有接口）
     * @param port 服务器监听的端口号
     * @param thread_pool_size 线程池大小，默认为 4
     */
    UdpServer(const std::string& ip, uint16_t port, size_t thread_pool_size = 4);
    
    /**
     * @brief 析构函数
     * @details 自动停止服务器并释放资源
     */
    ~UdpServer();
    
    /// @brief 禁止拷贝构造
    UdpServer(const UdpServer&) = delete;
    /// @brief 禁止拷贝赋值
    UdpServer& operator=(const UdpServer&) = delete;
    
    /**
     * @brief 启动服务器
     * @return true 启动成功，false 启动失败
     * 
     * @details
     * 启动流程：
     * 1. 创建 UDP socket
     * 2. 绑定地址和端口
     * 3. 启动接收线程
     */
    bool start();
    
    /**
     * @brief 停止服务器
     * 
     * @details
     * 停止流程：
     * 1. 关闭 socket
     * 2. 等待接收线程结束
     */
    void stop();
    
    /**
     * @brief 发送消息到指定地址
     * @param ip 目标 IP 地址
     * @param port 目标端口号
     * @param message 要发送的消息内容
     * @return true 发送成功，false 发送失败
     */
    bool send_to(const std::string& ip, uint16_t port, const std::string& message);
    
    /**
     * @brief 设置消息接收回调
     * @param callback 接收到消息时调用的回调函数
     */
    void set_message_callback(MessageCallback callback);
    
    /**
     * @brief 获取服务器运行状态
     * @return true 正在运行，false 已停止
     */
    bool is_running() const { return running_; }
    
private:
    /**
     * @brief 消息接收循环（在独立线程中运行）
     * @details 持续接收 UDP 数据报，并提交到线程池处理
     */
    void receive_loop();
    
    /**
     * @brief 处理接收到的消息（在线程池中运行）
     * @param sender_ip 发送方 IP 地址
     * @param sender_port 发送方端口号
     * @param message 接收到的消息内容
     */
    void process_message(const std::string& sender_ip, uint16_t sender_port, const std::string& message);
    
    std::string ip_;                                // 服务器绑定的 IP 地址
    uint16_t port_;                                 // 服务器监听的端口
    int socket_fd_;                                 // socket 文件描述符
    std::atomic<bool> running_;                     // 服务器运行状态标志
    
    std::unique_ptr<ThreadPool> thread_pool_;       // 线程池指针
    std::thread receive_thread_;                    // 接收消息的线程
    
    MessageCallback message_callback_;              // 消息接收回调
};

#endif // UDP_SERVER_H
