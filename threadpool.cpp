#include "threadpool.h"
#include <cstring>
#include <iostream>
#include <string>

using namespace std;

/********** * 任务队列定义 * **********/
TaskQueue::TaskQueue() { /* 构造函数 */
}

TaskQueue::~TaskQueue() { /* 析构函数 */
}

/* 添加任务 */
void TaskQueue::addTask(Task task) {
    mtx.lock();
    m_taskQ.emplace(task);
    mtx.unlock();
}

/* 添加任务 */
void TaskQueue::addTask(callback f, void *arg) {
    mtx.lock();
    m_taskQ.emplace(f, arg);
    mtx.unlock();
}

/* 获取任务 */
Task TaskQueue::takeTask() {
    Task task;
    mtx.lock();
    if (!m_taskQ.empty()) {
        task = m_taskQ.front();
        m_taskQ.pop();
    }
    mtx.unlock();
    return task;
}

/***************** * 线程池类的定义 * *****************/
ThreadPool::ThreadPool(int minNum, int maxNum) {
    taskQ = new TaskQueue; // 实例化一个任务队列
    this->minNum = minNum;
    this->maxNum = maxNum;
    busyNum = 0;
    aliveNum = minNum;
    work_threads = new thread[maxNum]; // 根据最大线程数申请空间
    /**************** * 根据最小线程数量创建工作线程 * ****************/
    for (int i = 0; i < minNum; i++) {
        work_threads[i] = thread(worker, this); // 创建工作线程
        cout << "创建子线程, ID: " << work_threads[i].get_id() << endl;
    }
    /*********** * 创建管理者线程 * ***********/
    manager_thread = thread(manager, this);
}

/* 退出一个线程 */
void ThreadPool::threadExit() {
    // TODO 本身应该是有一个线程来调用这个函数
    // 需要把调用这个函数的线程结束掉
    // 这个就有点麻烦了，thread在初始化的时候就和运行的函数绑定，运行完之后就自动销毁，
    // 所以没有退出一个线程这个说法
}
/* 管理者线程的任务函数 */
void *ThreadPool::manager(void *arg) {
    ThreadPool *pool = static_cast<ThreadPool *>(arg);
    while (!pool->shutdown) {
        // 建个3s检查一次
        this_thread::sleep_for(3s);
    }
}

/************* * 工作线程的任务函数 * *************/
void *ThreadPool::worker(void *arg) {
    // 有很多个worker，每个worker都是一个线程，执行队列中的任务
    // TODO 这里传入的this是实例对象的指针，但我的worker不是静态成员函数，
    // 所以即使不用传this也可以访问非静态的成员变量
    // 写完后试一下不传this指针行不行
    ThreadPool *pool = static_cast<ThreadPool *>(arg);
    while (true) {
        // 访问任务队列，加锁
        unique_lock<mutex> mut(pool->mutPool);
        mut.lock();
        // 判断任务个数，如果是0就阻塞
        while (pool->taskQ->taskNum() == 0 && !pool->shutdown) {
            cout << "thread " << this_thread::get_id() << " waiting..." << endl;
            notEmpty.wait(mut); // 阻塞线程

            // 解除阻塞后，判断是否需要销毁线程
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->aliveNum > pool->minNum) {
                    pool->aliveNum--;
                    mut.unlock();
                    pool->threadExit(); // TODO 退出哪个线程呢
                }
            }
        }

        // 判断线程池是否被关闭了
        if (pool->shutdown) {
            mut.unlock();
            pool->threadExit(); // 退出工作线程
        }

        // 从任务队列中取出一个任务
        Task task = pool->taskQ->takeTask();
        pool->busyNum++; // 工作线程+1
        mut.unlock();
        // 执行取出的任务
        cout << "thread " << this_thread::get_id() << " start working..." << endl;
        task.func(task.arg);
        delete task.arg;
        task.arg = nullptr;

        // 任务执行完毕
        cout << "thread " << this_thread::get_id() << " end working..." << endl;
        mut.lock();
        pool->busyNum--;
        mut.unlock();
    }
    return nullptr;
}