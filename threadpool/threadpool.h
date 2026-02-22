#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include<list>
#include<pthread.h>
#include"locker.h"

template<typename T>
class thread_pool{
    public:
        thread_pool(int thread_number = 8,int max_request = 10000);
        ~thread_pool();
        bool append(T *request);

    private:
        int m_thread_number;                /*线程池中的线程数*/
        int m_max_request;                  /*请求队列中允许的最大请求数*/
        pthread_t *m_threads;               /*描述线程池的数组,其大小为m_thread_number*/
        std::list<T*> m_workqueue;          /*请求队列*/
        locker m_queuelocker;               /*保护请求队列的互斥锁*/
        sem m_queuestat;                    /*是否有任务需要处理*/
        bool m_stop;                        /*是否结束线程*/

    private:
        static void *worker(void *arg);     /*工作线程运行的函数,它不断从工作队列中取出任务并执行*/
        void run();
};

template<typename T>
thread_pool<T>::thread_pool(int thread_number,int max_request):m_thread_number(thread_number),m_max_request(max_request),m_stop(false),m_threads(NULL){
    if(thread_number <= 0 || max_request <= 0){
        throw std::exception();
    }

    m_threads = new pthread_t[thread_number];
    if(!m_threads){
        throw std::exception();
    }

    /*创建m_thread_number个线程,并将他们设置为脱离线程*/
    for(int i = 0;i < m_thread_number;++i){
        printf("create the %dth thread\n",i);
        if(pthread_create(m_threads + i,NULL,worker,this)){
            delete [] m_threads;
            throw std::exception();
        }

        if(pthread_detach(m_threads[i])){
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
thread_pool<T>::~thread_pool(){
    delete [] m_threads;
    m_stop = true;
}

template<typename T>
bool thread_pool<T>::append(T *request){
    /*操作工作队列一定要加锁,因为他被所有线程共享*/
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void *thread_pool<T>::worker(void *argv){
    thread_pool* pool = (thread_pool *)argv;
    pool->run();
    return pool;
}

template<typename T>
void thread_pool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        request->process();
        
    }
}

#endif