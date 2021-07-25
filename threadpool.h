#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>
using namespace std;
/* 任务队列声明 */
template <typename T>

class TaskQueue {
  private:
    queue<T> m_taskQ; // 任务队列
    mutex mtx;        // 任务队列的互斥锁
  public:
    TaskQueue();
    ~TaskQueue();
    void addTask(T &t); // 添加任务
    // void addTask(callback f, void *arg);            // 添加任务
    T takeTask();                                   // 取任务
    inline int taskNum() { return m_taskQ.size(); } // 获取当前任务个数
    inline bool empty() { return m_taskQ.empty(); } // 队列是否为空
};

/* 线程池类声明 */

class ThreadPool {
  private:
    class ThreadWorker { // 内置的线程工作类
      private:
        int m_id;           // 工作ID
        ThreadPool *m_pool; // 所属的线程池
      public:
        ThreadWorker(ThreadPool *pool, const int id) : m_pool(pool), m_id(id) {}
        void operator()(); // 重载()操作
    };
    bool m_shutdown;                     // 线程池是否关闭
    TaskQueue<function<void()>> m_queue; // 线程池的任务队列
    vector<thread> work_threads;         // 工作线程队列
    mutex mtxPool;                       // 线程池互斥锁
    condition_variable condi_lock;       // 条件变量，可以让线程休眠或唤醒线程
  public:
    // 构造函数，传入线程数量
    ThreadPool(const int n_threads = 12) : work_threads(vector<thread>(n_threads)), m_shutdown(false) {}
    ~ThreadPool(); // 析构函数
    void init();   // 初始化线程池
    template <typename F, typename... Args>

    auto submit(F &&f, Args &&...args) -> future<decltype(f(args...))>; // 添加任务
    /*
      public:
        ThreadPool(int minNum, int maxNum);
        ~ThreadPool();
        // void addTask(T &t); // 添加任务
        int getBusyNum();  // 获取线程池中工作的线程数
        int getAliveNum(); // 获取线程池中活着的线程数
      private:
        void *worker(void *arg);  // 工作线程的任务函数
        void *manager(void *arg); // 管理者线程的任务函数
        void threadExit();        // 退出单个线程

      private:
        TaskQueue *taskQ;      // 建立一个任务队列指针
        thread manager_thread; // 管理者线程
        thread *work_threads;  // 工作线程池
        // unique_lock<mutex> mutPool; // 线程池锁
        mutex mutPool;                                   // 线程池锁
        condition_variable::condition_variable notEmpty; // 条件变量，指示队列是否为空
        int minNum;                                      // 最小线程数量
        int maxNum;                                      // 最大线程数量
        int busyNum;                                     // 忙的线程数量
        int aliveNum;                                    // 存活的线程数量
        int exitNum;                                     // 需要销毁的线程数量
        bool shutdown = false;                           // 是否要销毁线程池
     */
};
