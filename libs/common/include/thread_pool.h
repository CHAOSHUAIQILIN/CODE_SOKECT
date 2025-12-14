/**
 * @file thread_pool.h
 * @brief 线程池类的头文件
 * @author CSQL
 * @date 2025-12-14
 * @details
 * 提供一个通用的线程池实现，支持任务提交和异步执行。
 * 使用 C++17 标准，基于 std::thread 和 std::future 实现。
 * 
 * @note 线程池对象不可拷贝和移动
 * 
 * @example
 * @code
 * ThreadPool pool(4);  // 创建4个工作线程
 * auto future = pool.submit([](int x) { return x * 2; }, 10);
 * int result = future.get();  // result = 20
 * @endcode
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

/**
 * @class ThreadPool
 * @brief 线程池类，用于管理和调度多线程任务
 * 
 * @details
 * 线程池在构造时创建指定数量的工作线程，这些线程会从任务队列中
 * 获取任务并执行。支持任意可调用对象的提交，并返回 std::future
 * 以获取异步结果。
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数，创建线程池
     * @param num_threads 工作线程数量，默认为 CPU 核心数
     * 
     * @details 创建指定数量的工作线程，并立即开始等待任务
     */
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    
    /**
     * @brief 析构函数
     * @details 调用 shutdown() 停止所有线程并等待它们完成
     */
    ~ThreadPool();
    
    /// @brief 禁止拷贝构造
    ThreadPool(const ThreadPool&) = delete;
    /// @brief 禁止拷贝赋值
    ThreadPool& operator=(const ThreadPool&) = delete;
    /// @brief 禁止移动构造
    ThreadPool(ThreadPool&&) = delete;
    /// @brief 禁止移动赋值
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    /**
     * @brief 提交任务到线程池
     * @tparam F 可调用对象类型
     * @tparam Args 参数类型包
     * @param f 要执行的可调用对象（函数、lambda、仿函数等）
     * @param args 传递给可调用对象的参数
     * @return std::future<返回类型> 用于获取任务执行结果的 future 对象
     * 
     * @throws std::runtime_error 如果线程池已经关闭
     * 
     * @example
     * @code
     * auto future = pool.submit([](int a, int b) { return a + b; }, 1, 2);
     * int sum = future.get();  // sum = 3
     * @endcode
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;
    
    /**
     * @brief 获取线程池中的线程数量
     * @return 工作线程数量
     */
    size_t size() const { return workers_.size(); }
    
    /**
     * @brief 获取待处理的任务数量
     * @return 任务队列中等待执行的任务数量
     */
    size_t pending_tasks() const;
    
    /**
     * @brief 关闭线程池
     * @details 停止接受新任务，等待所有已提交的任务完成后关闭所有线程
     */
    void shutdown();
    
private:
    std::vector<std::thread> workers_;              // 工作线程容器
    std::queue<std::function<void()>> tasks_;       // 任务队列
    
    mutable std::mutex queue_mutex_;                // 任务队列互斥锁
    std::condition_variable condition_;             // 条件变量，用于线程同步
    std::atomic<bool> stop_;                        // 线程池停止标志
};

/**
 * @brief 提交任务的模板函数实现
 * @details 模板函数必须在头文件中实现
 */
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using return_type = decltype(f(args...));
    
    // 将任务包装为 shared_ptr<packaged_task>
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 检查线程池是否已关闭
        if (stop_) {
            throw std::runtime_error("ThreadPool: cannot submit task after shutdown");
        }
        
        // 将任务添加到队列
        tasks_.emplace([task]() { (*task)(); });
    }
    
    // 通知一个等待的工作线程
    condition_.notify_one();
    return result;
}

#endif // THREAD_POOL_H
