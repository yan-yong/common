#ifndef __CQUEUE_H_
#define __CQUEUE_H_

#include <queue>
#include <string>
#include "lock/lock.hpp"
#include "log/log.h"

template<typename DataType, typename QueueType = std::queue<DataType> >
class CQueue
{
protected:
    bool __enqueue(const DataType& data)
    {
        MutexGuard guard(m_enqueue_mutex);
        m_enqueue_data->push(data);
        m_data_semaphore.post();
        __sync_fetch_and_add(&m_size, 1);
        return true;
    }

    bool __dequeue(DataType& data)
    {
        if(m_exit && m_size == 0)
            return false;
        MutexGuard guard(m_dequeue_mutex);
        //error!! should never occur
        if(m_dequeue_data->size() + m_enqueue_data->size() == 0)
        {
            LOG_ERROR("CQueue::Dequeue: Crazy error\n");
            return false;
        }
        if(m_dequeue_data->size() == 0 && m_enqueue_data->size() != 0)
        {
            MutexGuard guard(m_enqueue_mutex);
            std::swap(m_dequeue_data, m_enqueue_data);
        }
        data =  m_dequeue_data->front();
        m_dequeue_data->pop();
        m_empty_semaphore.post();
        __sync_fetch_and_sub(&m_size, 1);
        return true;
    }

public:
	CQueue(size_t max_size = 1000000): m_size(0), m_max_size(max_size),
        m_data_semaphore(0), m_empty_semaphore(max_size), m_exit(false) 
    {
        m_enqueue_data = new QueueType;
        m_dequeue_data = new QueueType;
    }

	virtual ~CQueue()
    {
        LOG_DEBUG("Enter CQueue destruct %p\n", this);
        exit();
        delete m_enqueue_data;
        m_enqueue_data = NULL;
        delete m_dequeue_data;
        m_dequeue_data = NULL;
        LOG_DEBUG("Leave CQueue destruct %p\n", this);
    }

    void set_max_size(size_t max_size)
    {
        //boost have no such interface, so loop ...
        while(max_size > m_max_size){
            m_empty_semaphore.post();
            m_max_size++;  
        }
        while(max_size < m_max_size){
            m_empty_semaphore.try_wait();
            m_max_size--;
        }
    }

	size_t size()
    {
        return m_size;
    }

	bool enqueue(const DataType& data)
    {
        if(m_exit) return false;
        m_empty_semaphore.wait();
        if(m_exit) return false;
        return __enqueue(data);
    }
	bool timed_enqueue(const DataType& data, boost::posix_time::time_duration dura)
    {
        boost::posix_time::ptime timeout = boost::get_system_time() + dura; 
        if(m_exit || !m_empty_semaphore.timed_wait(timeout) || m_exit)
            return false;
        return __enqueue(data); 
    }
	bool try_enqueue(const DataType& data)
    {
        if(m_exit || m_empty_semaphore.try_wait() || m_exit) 
            return false;
        return __enqueue(data); 
    }

    //no wait
    bool try_dequeue(DataType& data)
    {
        if(!m_data_semaphore.try_wait()){
            return false;
        }
        return __dequeue(data);
    }
	bool timed_dequeue(DataType& data, boost::posix_time::time_duration dura)
    {
        if(m_exit)
            return try_dequeue(data);
        boost::posix_time::ptime timeout = boost::get_system_time() + dura;
        if(!m_data_semaphore.timed_wait(timeout)) 
            return false;
        return __dequeue(data);
    }
	bool dequeue(DataType& data)
    {
        if(m_exit){
            return try_dequeue(data);
        }
        
        m_data_semaphore.wait();
        return __dequeue(data);
    }

    void exit()
    {
        if(!__sync_bool_compare_and_swap(&m_exit, false, true)){
            //LOG_INFO("CQueue exit %p dumplicate.\n", this);
            return;
        }
        LOG_INFO("CQueue exiting ... \n");
        do{
            m_data_semaphore.post();
            usleep(10000);
        }while(!m_data_semaphore.try_wait());

        do{
            m_empty_semaphore.post();
            usleep(10000);
        }while(!m_empty_semaphore.try_wait());
        LOG_INFO("CQueue exited.\n");
    }

protected:
	volatile size_t m_size;
    size_t m_max_size;
    QueueType * m_enqueue_data;
    QueueType * m_dequeue_data;
    Mutex m_enqueue_mutex;
	Mutex m_dequeue_mutex;
	Semaphore m_data_semaphore;
    Semaphore m_empty_semaphore;
    bool m_exit;
};

#endif
