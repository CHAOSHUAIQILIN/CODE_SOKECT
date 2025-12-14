/**
 * @file udp_client.h
 * @brief UDP 客户端类的头文件
 * @author Your Name
 * @date 2024
 * @version 1.0.0
 * 
 * @details
 * 提供 UDP 客户端功能，支持：
 * - 向指定地址发送 UDP 数据报
 * - 接收来自任意地址的 UDP 数据报
 * - 通过回调处理接收到的消息
 * 
 * @note
 * - 该类不可拷贝
 * - UDP 是无连接协议，不保证消息送达
 * 
 * @example
 * @code
 * UdpClient client;
 * client.init(0);  // 使用任意可用端口
 * client.set_message_callback([](const std::string& ip, uint16_t port, const std::string& msg) {
 *     std::cout << "From " << ip << ":" << port << " - " << msg << std::endl;
 * });
 * client.start_receiving();
 * client.send_to("127.0.0.1", 8080, "Hello!");
 * @endcode
 */

#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

/**
 * @class UdpClient
 * @brief UDP 客户端类，用于发送和接收 UDP 数据报
 * 
 * @details
 * 该类封装了 UDP 客户端的基本功能：
 * - 初始化并可选绑定本地端口
 * - 向指定地址发送数据报
 * - 在后台线程接收数据报
 * - 通过回调通知接收到的消息
 */
class UdpClient {
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
     * @details 初始化客户端对象，但不创建 socket
     */
    UdpClient();
    
    /**
     * @brief 析构函数
     * @details 自动关闭 socket 并释放资源
     */
    ~UdpClient();
    
    /// @brief 禁止拷贝构造
    UdpClient(const UdpClient&) = delete;
    /// @brief 禁止拷贝赋值
    UdpClient& operator=(const UdpClient&) = delete;
    
    /**
     * @brief 初始化客户端
     * @param local_port 本地绑定端口，0 表示由系统分配
     * @return true 初始化成功，false 初始化失败
     * 
     * @details
     * 创建 UDP socket，如果指定了端口则绑定到该端口。
     * 绑定端口后可以接收发送到该端口的数据报。
     */
    bool init(uint16_t local_port = 0);
    
    /**
     * @brief 关闭客户端
     * @details 停止接收并关闭 socket
     */
    void close();
    
    /**
     * @brief 发送消息到指定地址
     * @param ip 目标 IP 地址
     * @param port 目标端口号
     * @param message 要发送的消息内容
     * @return true 发送成功，false 发送失败
     * 
     * @note 该函数是线程安全的
     */
    bool send_to(const std::string& ip, uint16_t port, const std::string& message);
    
    /**
     * @brief 开始接收消息
     * @details 启动后台接收线程
     */
    void start_receiving();
    
    /**
     * @brief 停止接收消息
     * @details 停止后台接收线程
     */
    void stop_receiving();
    
    /**
     * @brief 设置消息接收回调
     * @param callback 接收到消息时调用的回调函数
     */
    void set_message_callback(MessageCallback callback);
    
    /**
     * @brief 获取初始化状态
     * @return true 已初始化，false 未初始化
     */
    bool is_initialized() const { return initialized_; }
    
    /**
     * @brief 获取接收状态
     * @return true 正在接收，false 未接收
     */
    bool is_receiving() const { return receiving_; }
    
private:
    /**
     * @brief 消息接收循环（在独立线程中运行）
     * @details 持续接收 UDP 数据报，直到调用 stop_receiving()
     */
    void receive_loop();
    
    int socket_fd_;                         // socket 文件描述符
    std::atomic<bool> initialized_;         // 初始化状态标志
    std::atomic<bool> receiving_;           // 接收状态标志
    std::thread receive_thread_;            // 接收消息的线程
    std::mutex send_mutex_;                 // 发送操作的互斥锁
    
    MessageCallback message_callback_;      // 消息接收回调
};

#endif // UDP_CLIENT_H
