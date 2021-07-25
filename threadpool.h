#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

using callback = void (*)(void *arg); // 定义任务函数别名

class Task {
  public:
    void (*func)(void *arg); // 定义任务函数指针
    void *arg;               // 定义任务函数参数
    Task() {                 //无参构造
        func = nullptr;
        arg = nullptr;
    }
    Task(callback f, void *arg) { // 定义有参构造函数
        this->func = f;
        this->arg = arg;
    }
};

/* 任务队列声明 */
template <typename T> class TaskQueue {
  public:
    TaskQueue();
    ~TaskQueue();
    void addTask(Task task);                        // 添加任务
    void addTask(callback f, void *arg);            // 添加任务
    Task takeTask();                                // 取任务
    inline int taskNum() { return m_taskQ.size(); } // 获取当前任务个数
    inline bool empty() { return m_taskQ.empty(); } // 队列是否为空

  private:
    std::queue<Task> m_taskQ; // 任务队列
    std::mutex mtx;           // 任务队列的互斥锁
};

/* 线程池类声明 */
class ThreadPool {
  public:
    ThreadPool(int minNum, int maxNum);
    ~ThreadPool();
    void addTask(Task task); // 添加任务
    int getBusyNum();        // 获取线程池中工作的线程数
    int getAliveNum();       // 获取线程池中活着的线程数
  private:
    void *worker(void *arg);  // 工作线程的任务函数
    void *manager(void *arg); // 管理者线程的任务函数
    void threadExit();        // 退出单个线程

  private:
    TaskQueue *taskQ;           // 建立一个任务队列指针
    std::thread manager_thread; // 管理者线程
    std::thread *work_threads;  // 工作线程池
    // std::unique_lock<mutex> mutPool; // 线程池锁
    std::mutex mutPool;                                   // 线程池锁
    std::condition_variable::condition_variable notEmpty; // 条件变量，指示队列是否为空
    int minNum;                                           // 最小线程数量
    int maxNum;                                           // 最大线程数量
    int busyNum;                                          // 忙的线程数量
    int aliveNum;                                         // 存活的线程数量
    int exitNum;                                          // 需要销毁的线程数量
    bool shutdown = false;                                // 是否要销毁线程池
};
