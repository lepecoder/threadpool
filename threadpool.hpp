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
    bool m_shutdown;                     // 线程池关闭标记
    TaskQueue<function<void()>> m_queue; // 线程池的任务队列
    vector<thread> work_threads;         // 工作线程队列
    mutex mtxPool;                       // 线程池互斥锁
    condition_variable condi_lock;       // 条件变量，可以让线程休眠或唤醒线程
  public:
    // 构造函数，传入线程数量
    ThreadPool(const int n_threads = 12) : work_threads(vector<thread>(n_threads)), m_shutdown(false) {}
    // ~ThreadPool();   // 析构函数
    void init();     // 初始化线程池
    void shutdown(); // 关闭线程池
    template <typename F, typename... Args>

    auto submit(F &&f, Args &&...args) -> future<decltype(f(args...))>; // 添加任务
};
void ThreadPool::init() {
    // 为工作线程分配工人
    for (int i = 0; i < work_threads.size(); i++) {
        work_threads.at(i) = thread(ThreadWorker(this, i));
    }
}

void ThreadPool::ThreadWorker::operator()() {
    // 第一个括号是要重载的运算符，第二个括号是形参列表
    // function<void()> func;  // 基础函数类func
    // bool dequeued;  // 是否正在取出队列中的元素
    while (!m_pool->m_shutdown) {
        // {
        // 互斥的访问工作线程队列 work_threads
        unique_lock<mutex> lock(m_pool->mtxPool);
        // 如果任务队列空，阻塞当前线程，直到条件变量唤醒
        if (m_pool->m_queue.empty()) {
            m_pool->condi_lock.wait(lock);
        }
        // 取出任务队列中的任务
        auto task_func = m_pool->m_queue.takeTask();
        if (task_func != nullptr) {
            task_func();
        }
        // }
    }
}

void ThreadPool::shutdown() {
    // 关闭线程池
    m_shutdown = true;
    condi_lock.notify_all(); // 信号量通知，唤醒所有工作线程
    for (auto &thread : work_threads) {
        if (thread.joinable()) { // 还没有join，而且是活跃的线程
            thread.join();       // 等待执行结束
        }
    }
}

template <typename F, typename... Args> auto ThreadPool::submit(F &&f, Args &&...args) -> future<decltype(f(args...))> {
    /* 向线程池添加一个任务, 返回future异步对象 */
    // 创建一个函数
    function<decltype(f(args...))()> func = bind(forward<F>(f), forward<Args>(args)...);
    // 返回packaged_task的共享指针，packaged_task可以包装函数，使得能异步调用它
    // 通过get_future可以返回future对象
    auto task_ptr = make_shared<packaged_task<decltype(f(args...))()>>(func);
    // 打包成 void function 通用函数
    function<void()> warpper_func = [task_ptr]() { (*task_ptr)(); };
    // 加到任务队列里
    m_queue.addTask(warpper_func);
    // 唤醒一个等待中的线程
    condi_lock.notify_one();
    // 返回任务的future对象
    return task_ptr->get_future();
}

template <typename T> TaskQueue<T>::TaskQueue() {
    // 构造函数
}
template <typename T> TaskQueue<T>::~TaskQueue() {
    // 析构函数
}

template <typename T> void TaskQueue<T>::addTask(T &t) {
    // 添加任务
    unique_lock<mutex> lock(mtx);
    m_taskQ.emplace(t);
}

template <typename T> T TaskQueue<T>::takeTask() {
    // 取出任务
    unique_lock<mutex> lock(mtx); // 加锁
    if (m_taskQ.empty())
        return nullptr;
    T res = move(m_taskQ.front()); // 移动语义
    m_taskQ.pop();
    return res;
}
