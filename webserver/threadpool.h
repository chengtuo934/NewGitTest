#ifndef THREADPOOL.H 
#define THREADPOOL.H 

#include <pthread.h>
#include <list>
#include "locker.h"
#include <exception>
#include <stdio.h>
using namespace std;

// 线程池类， 定义成模板类是为了代码的复用，模板参数T是任务类
template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    static void* worker(void* arg);
    void run();

private:
    // 线程的数量
    int m_thread_number;

    // 线程池数组，大小为m_thread_number
    pthread_t * m_threads;

    // 请求队列中最多允许的等待处理的请求数量
    int m_max_requests;

    // 请求队列
    list<T*> m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    // 信号量用来判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;

};

// 类外实现构造函数
template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_requests(max_requests),
    m_stop(false), m_threads(NULL) {
    
    if ((thread_number <= 0) || (max_requests <= 0)) {
        throw exception();
    }

    m_threads = new pthread_t[m_thread_number];
    // 判断数组是否创建成功
    if (!m_threads) {
        throw exception();
    }

    // 创建thread_number个线程， 并将它们设置为线程脱离(自己释放资源)
    for (int i = 0; i < thread_number; ++i) {
        printf("create the %dth thread\n", i);

        // 创建线程， worker必须是个静态函数
        // m_thread + i 是首元素加偏移量，即地址
        // 将this作为参数传递给worker，静态函数worker就可以通过this使用非静态成员了
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete [] m_threads;
            throw exception();
        }

        // 设置线程分离
        if (pthread_detach(m_threads[i])) {
            delete [] m_threads;
            throw exception();
        }

    }
}

// 析构函数
template<typename T>
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

// append函数
template<typename T>
bool threadpool<T>::append(T * request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// worker函数
template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*) arg;  // arg是this
    pool->run();
    return pool;
}

// run函数
template<typename T>
void threadpool<T>::run() {
    while(!m_stop) {
        // 检测信号量
        m_queuestat.wait();
        // 加锁
        m_queuelocker.lock();
        
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        request->process();
    }
}

#endif
