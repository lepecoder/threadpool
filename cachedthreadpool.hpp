#pragma once
// #include <condition_variable>
#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
/*****************
 * 可缓存线程池        *
 * 指定最大和最小的线程数量， *
 * 根据任务数量灵活增减线程  *
 *****************/
using namespace std;
/* 任务队列声明 */
template <typename T>

class TaskQueue {
  private:
    queue<T> m_taskQ;  // 任务队列
    mutex m_taskQ_mtx; // 任务队列的互斥锁
  public:
    TaskQueue();
    ~TaskQueue();
    void addTask(T &t); // 添加任务
    T takeTask();       // 取任务
    int size();         // 获取当前任务个数
    bool empty();       // 队列是否为空
};
template <typename T> int TaskQueue<T>::size() {
    // 任务队列中的任务数
    unique_lock<mutex> lock(m_taskQ_mtx);
    return m_taskQ.size();
}

template <typename T> bool TaskQueue<T>::empty() {
    // 任务队列是否为空
    unique_lock<mutex> lock(m_taskQ_mtx);
    return m_taskQ.empty();
}

template <typename T> TaskQueue<T>::TaskQueue() {
    // 构造函数
}
template <typename T> TaskQueue<T>::~TaskQueue() {
    // 析构函数
}

template <typename T> void TaskQueue<T>::addTask(T &t) {
    // 添加任务
    unique_lock<mutex> lock(m_taskQ_mtx);
    m_taskQ.emplace(t);
}

template <typename T> T TaskQueue<T>::takeTask() {
    // 取出任务
    unique_lock<mutex> lock(m_taskQ_mtx); // 加锁
    if (m_taskQ.empty())
        return nullptr;
    T res = move(m_taskQ.front()); // 移动语义
    m_taskQ.pop();
    return res;
}

/* 线程池类声明 */

class ThreadPool {
  private:
    class ThreadWorker { // 内置的线程工作类
      private:
        thread::id m_id;    // 工作ID
        bool is_dead;       // 线程是否死亡
        ThreadPool *m_pool; // 所属的线程池
      public:
        ThreadWorker(ThreadPool *pool) : m_pool(pool), is_dead(false), m_id(this_thread::get_id()) {}
        void operator()(); // 重载()操作
    };
    bool m_shutdown;                         // 线程池关闭标记
    TaskQueue<function<void()>> tasks_queue; // 线程池的任务队列
    // vector<thread> work_threads;             // 工作线程队列
    unordered_map<thread::id, thread> work_threads; // 工作线程队列
    mutex mtxPool;                                  // 线程池互斥锁
    mutex mtxTask;                                  // 等待任务队列为空
    condition_variable condi_var_mtxPool;           // 条件变量，可以让线程休眠或唤醒线程
    condition_variable condi_var_mtxTask;           //
    int minNums;                                    // 最小线程数
    int maxNums;                                    // 最大线程数
    // int countThread;                         // 增加线程计数
    atomic<int> freeNums;                       // 空闲线程数
    atomic<int> threadNums;                     // 当前线程数
    unordered_set<thread::id> finished_threads; // 需要关闭的线程id

  public:
    // 构造函数，传入线程数量
    ThreadPool(const int minNums, const int maxNums);
    // ~ThreadPool();   // 析构函数
    void wait();
    void shutdown(); // 关闭线程池

    template <typename F, typename... Args> auto submit(F &&f, Args &&...args) -> future<decltype(f(args...))>; // 添加任务
};

ThreadPool::ThreadPool(const int minNums, const int maxNums) {
    this->minNums = minNums;
    this->maxNums = maxNums;
    this->freeNums = minNums; // 初始时所有线程都是空闲的
    threadNums = 0;
    m_shutdown = false;
    // work_threads = vector<thread>(minNums);
    for (int i = 0; i < minNums; i++) {
        thread t(ThreadWorker(this));
        assert(work_threads.find(t.get_id()) == work_threads.end());
        work_threads[t.get_id()] = move(t);
        // work_threads.at(i) = thread(ThreadWorker(this));
        threadNums++;
    }
    // cout << "构造函数结束\n";
}

void ThreadPool::ThreadWorker::operator()() {
    // 第一个括号是要重载的运算符，第二个括号是形参列表
    // function<void()> func;  // 基础函数类func
    // bool dequeued;  // 是否正在取出队列中的元素
    while (!m_pool->m_shutdown && is_dead == false) {
        unique_lock<mutex> lock(m_pool->mtxPool);
        /* 从线程队列中销毁死亡的线程*/
        while (!m_pool->finished_threads.empty()) { // finished_threads中存储死亡的线程id
            for (auto it = m_pool->work_threads.begin(); it != m_pool->work_threads.end(); it++) {
                auto idx = m_pool->finished_threads.find(it->second.get_id());
                if (idx != m_pool->finished_threads.end()) { // 如果id对得上
                    // 这时候it这个线程应该已经把函数运行完了
                    cout << "移除死亡线程  " << it->second.get_id() << endl;
                    if (it->second.joinable()) {
                        it->second.join();
                    }
                    m_pool->work_threads.erase(it);      // 从线程队列中移除
                    m_pool->finished_threads.erase(idx); // 从死亡id集合中移除
                }
            }
        }

        // 如果任务队列空，阻塞当前线程，直到条件变量唤醒
        if (m_pool->tasks_queue.empty()) {
            m_pool->condi_var_mtxTask.notify_one(); // 通知wait
            // 任务队列为空，并且工作线程数大于线程池允许的最小值，可以减少工作线程数
            if (m_pool->threadNums > m_pool->minNums) {
                cout << "threadpool--, 当前线程数 " << m_pool->work_threads.size() << "  threadNums " << m_pool->threadNums << endl;
                // TODO 减少之后，虽然线程thread关闭了，但是还没有从vector中移出
                m_pool->finished_threads.emplace(this_thread::get_id());
                is_dead = true;
                m_pool->threadNums--; // 队列中线程数-1
                m_pool->freeNums--;   // 空闲线程数-1
                break;                // 直接退出while循环 线程就会自动销毁
            }
            m_pool->condi_var_mtxPool.wait(lock);
        }
        // 取出任务队列中的任务
        auto task_func = m_pool->tasks_queue.takeTask();
        lock.unlock();
        if (task_func != nullptr) {
            m_pool->freeNums--;
            task_func();
            m_pool->freeNums++;
        }
    }
}

void ThreadPool::wait() {
    // 等待任务队列中的任务执行完毕
    unique_lock<mutex> lock(mtxTask);
    if (!tasks_queue.empty()) {
        condi_var_mtxTask.wait(lock);
    }
    return;
}

void ThreadPool::shutdown() {
    // 关闭线程池
    m_shutdown = true;
    condi_var_mtxPool.notify_all(); // 信号量通知，唤醒所有工作线程
    for (auto &thread : work_threads) {
        if (thread.second.joinable()) { // 还没有join，而且是活跃的线程
            thread.second.join();       // 等待执行结束
        }
    }
}

template <typename F, typename... Args> auto ThreadPool::submit(F &&f, Args &&...args) -> future<decltype(f(args...))> {
    /* 向线程池添加一个任务, 返回future异步对象 */
    if (freeNums < 1 && work_threads.size() < maxNums) { //  空闲线程小于1，并且工作线程数小于允许的最大线程数, 就可以新建线程
        thread t(ThreadWorker(this));
        assert(work_threads.find(t.get_id()) == work_threads.end());
        work_threads[t.get_id()] = move(t);
        threadNums++;
        // work_threads.emplace_back(thread(ThreadWorker(this)));
        freeNums++; // 空闲线程数+1
        cout << "thread add 1 " << work_threads.size() << endl;
    }
    // 创建一个函数
    function<decltype(f(args...))()> func = bind(forward<F>(f), forward<Args>(args)...);
    // 返回packaged_task的共享指针，packaged_task可以包装函数，使得能异步调用它
    // 通过get_future可以返回future对象
    auto task_ptr = make_shared<packaged_task<decltype(f(args...))()>>(func);
    // 打包成 void function 通用函数
    function<void()> warpper_func = [task_ptr]() { (*task_ptr)(); };
    // 加到任务队列里
    tasks_queue.addTask(warpper_func);
    // 唤醒一个等待中的线程
    condi_var_mtxPool.notify_one();
    // 返回任务的future对象
    return task_ptr->get_future();
}
