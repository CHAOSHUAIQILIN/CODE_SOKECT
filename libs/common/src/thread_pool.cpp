#include "thread_pool.h"

/**
 * @brief 构造函数实现
 * @param num_threads 要创建的工作线程数量
 * 
 * @details
 * 创建指定数量的工作线程，每个线程执行以下逻辑：
 * 1. 等待条件变量通知
 * 2. 从任务队列获取任务
 * 3. 执行任务
 * 4. 重复上述过程直到线程池关闭
 */
ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    
                    // 等待条件：线程池停止 或 任务队列非空
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    
                    // 如果线程池已停止且没有待处理任务，退出线程
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    
                    // 从队列获取任务
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                // 执行任务（在锁外执行，避免阻塞其他线程）
                task();
            }
        });
    }
}

/**
 * @brief 析构函数实现
 * @details 调用 shutdown() 确保所有线程正确关闭
 */
ThreadPool::~ThreadPool() {
    shutdown();
}

/**
 * @brief 获取待处理任务数量
 * @return 任务队列中的任务数量
 * 
 * @note 该函数是线程安全的
 */
size_t ThreadPool::pending_tasks() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

/**
 * @brief 关闭线程池
 * 
 * @details
 * 关闭流程：
 * 1. 设置停止标志
 * 2. 通知所有等待的工作线程
 * 3. 等待所有线程完成当前任务并退出
 * 
 * @note 
 * - 该函数可以安全地多次调用
 * - 调用后不能再提交新任务
 */
void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        // 避免重复关闭
        if (stop_) {
            return;
        }
        stop_ = true;
    }
    
    // 唤醒所有等待的线程
    condition_.notify_all();
    
    // 等待所有工作线程结束
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}
