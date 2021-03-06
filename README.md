# 线程池

在TinyHttpdcpp中我们用C++实现了一个简单的多线程web服务器，每当服务器接收到一个请求时，它都会创建一个单独线程来处理请求。

第一个问题是创建线程所需的时间多少，以及线程在完成工作之后会被丢弃的事实。第二个问题更为麻烦，如果允许所有并发请求都通过新线程来处理，那么我们没有限制系统内的并发执行线程的数量。无限制的线程可能耗尽系统资源，如 CPU 时间和内存。解决这个问题的一种方法是使用**线程池**。

线程池的主要思想是：在进程开始时创建一定数量的线程，并加到池中以等待工作。当服务器收到请求时，它会唤醒池内的一个线程（如果有可用线程），并将需要服务的请求传递给它。一旦线程完成了服务，它会返回到池中再等待工作。如果池内没有可用线程，那么服务器会等待，直到有空线程为止。

但是在C++的`thread`里都是执行一个固定的task函数，执行结束后线程也自动结束，不能像`pthread.h`中把线程唤醒执行其它的任务，一个简单的想法是让`thread`执行调度函数，调度函数负责从任务队列中取出任务执行，任务会结束，但调度函数是一个死循环，直到线程池被关闭。

所以在C++里的线程池的实现是**让每一个thread创建后，就去执行调度函数：循环获取task，然后执行。**

![img](.assert/v2-f9350f72ee96164cc85d8e09c4a3a0d9_720w.jpg)


线程池具有以下优点：

1. 用现有线程服务请求比等待创建一个线程更快。
2. 线程池限制了任何时候可用线程的数量。这对那些不能支持大量并发线程的系统非常重要。
3. 将要执行任务从创建任务的机制中分离出来，允许我们采用不同策略运行任务。例如，任务可以被安排在某一个时间延迟后执行，或定期执行。


### threadpool.hpp
`threadpool.hpp`是固定线程数的线程池，有任务队列类和线程池类。任务队列类负责维护一个任务队列，并且提供线程安全的添加任务和获取任务。线程池类负责将传入的任务包装成统一格式提交到任务队列并唤醒一个等待中的线程，之后返回任务指针，我们可以通过这个任务指针获取任务的执行后的返回结果。

### cachedthreadpool.hpp

自动增减线程的线程池版本，任务队列为空时减少线程数，直到下界；


### 线程池流程

1. 构造线程池对象，传入线程的数量，构造函数会创建一个`vector<thread>`.
2. 初始化线程池`init`，为`vector`中的每个线程分配一个`work`对象，这个对象保存了线程池的信息，并重载了`()`运算符，因此线程创建之初就会执行`()`重载函数定义的操作，`work`对象也可以写一个管理函数代替或是匿名函数。
3. `work`对象会先检查线程池关闭标记，如果没有关闭的话就从任务队列中获取任务，如果任务队列为空，就用条件变量阻塞，在添加任务时会通知条件变量唤醒。虽然获取任务的队列操作是互斥的，但是需要将锁传给条件变量。取出任务之后`work`就可以执行任务。
4. `submit`操作向线程池中添加任务，它可以接收任何类型的函数，包括Lambda表达式，成员函数。可以立即返回`future`对象，避免阻塞线程。创建者可以通过`future`对象获取执行结果。
5. 等待任务队列中的线程执行结束
6. 关闭线程池，等待所有线程执行结束后，关闭线程池，但是任务队列中的任务可能没有得到执行。

### 遇到的问题

线程池的自动增加还是挺简单的，可以增加一个原子变量记录空闲线程数，在提交任务时根据空闲线程数和当前线程数，新建一个线程放入线程队列。

何时减小线程是一个问题，虽然`thread`在执行完任务函数时会自动销毁，但是如何从线程队列中删除它却值得考虑，我的解决方案是增加一个`finished_threads`集合，当一个线程需要被销毁时就将它的id加入集合，在线程的工作函数中检查`finished_threads`集合，如果不为空就从线程队列中删除匹配的线程。

