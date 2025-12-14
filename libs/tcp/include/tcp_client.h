/**
 * @file tcp_client.h
 * @brief TCP 客户端类的头文件
 * @author CSQL
 * @date 2025-12-14
 * 
 * @details
 * 提供 TCP 客户端功能，支持连接服务器、发送消息、接收消息。
 * 使用回调机制处理接收到的消息和连接状态变化。
 * 
 * @note 
 * - 该类不可拷贝
 * - 消息接收在独立线程中进行
 * 
 * @example
 * @code
 * TcpClient client;
 * client.set_message_callback([](const std::string& msg) {
 *     std::cout << "Received: " << msg << std::endl;
 * });
 * client.connect("127.0.0.1", 8080);
 * client.send("Hello, Server!");
 * @endcode
 */

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

/**
 * @class TcpClient
 * @brief TCP 客户端类，用于连接服务器并进行通信
 * 
 * @details
 * 该类封装了 TCP 客户端的基本功能：
 * - 连接到指定的服务器地址和端口
 * - 发送字符串消息
 * - 在后台线程接收消息
 * - 通过回调通知消息接收和连接状态变化
 */
class TcpClient {
public:
    /**
     * @brief 消息接收回调函数类型
     * @param message 接收到的消息内容
     */
    using MessageCallback = std::function<void(const std::string& message)>;
    
    /**
     * @brief 连接状态变化回调函数类型
     * @param connected true 表示已连接，false 表示已断开
     */
    using ConnectionCallback = std::function<void(bool connected)>;
    
    /**
     * @brief 构造函数
     * @details 初始化客户端，但不进行连接
     */
    TcpClient();
    
    /**
     * @brief 析构函数
     * @details 自动断开连接并释放资源
     */
    ~TcpClient();
    
    /// @brief 禁止拷贝构造
    TcpClient(const TcpClient&) = delete;
    /// @brief 禁止拷贝赋值
    TcpClient& operator=(const TcpClient&) = delete;
    
    /**
     * @brief 连接到服务器
     * @param ip 服务器 IP 地址（IPv4 格式，如 "192.168.1.1"）
     * @param port 服务器端口号
     * @return true 连接成功，false 连接失败
     * 
     * @details
     * 连接成功后会：
     * 1. 调用连接回调（如果已设置）
     * 2. 启动后台接收线程
     * 
     * @note 如果已经连接，调用此函数会返回 false
     */
    bool connect(const std::string& ip, uint16_t port);
    
    /**
     * @brief 断开与服务器的连接
     * 
     * @details
     * 断开连接后会：
     * 1. 关闭 socket
     * 2. 等待接收线程结束
     * 3. 调用连接回调（如果已设置）
     */
    void disconnect();
    
    /**
     * @brief 发送消息到服务器
     * @param message 要发送的消息内容
     * @return true 发送成功，false 发送失败或未连接
     * 
     * @note 该函数是线程安全的
     */
    bool send(const std::string& message);
    
    /**
     * @brief 设置消息接收回调
     * @param callback 接收到消息时调用的回调函数
     */
    void set_message_callback(MessageCallback callback);
    
    /**
     * @brief 设置连接状态变化回调
     * @param callback 连接状态变化时调用的回调函数
     */
    void set_connection_callback(ConnectionCallback callback);
    
    /**
     * @brief 获取当前连接状态
     * @return true 已连接，false 未连接
     */
    bool is_connected() const { return connected_; }
    
private:
    /**
     * @brief 消息接收循环（在独立线程中运行）
     * @details 持续从 socket 接收数据，直到连接断开
     */
    void receive_loop();
    
    int socket_fd_;                         // socket 文件描述符
    std::atomic<bool> connected_;           // 连接状态标志
    std::thread receive_thread_;            // 接收消息的线程
    std::mutex send_mutex_;                 // 发送操作的互斥锁
    
    MessageCallback message_callback_;      // 消息接收回调
    ConnectionCallback connection_callback_;// 连接状态回调
};

#endif // TCP_CLIENT_H
