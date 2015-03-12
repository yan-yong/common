#ifndef __CPRIORITY_QUEUE_H_
#define __CPRIORITY_QUEUE_H_

#include "CQueue.h"

template<typename DataType, typename QueueType = std::queue<DataType> >
class CPriorityQueue
{
protected:
    typedef CQueue<DataType, QueueType> ElemType;
    //this map will only be read when executing, so need no lock
    bool m_exit;
    size_t   m_size;
    unsigned m_priority_num;
    unsigned m_max_size;
    ElemType** m_queues;
	Semaphore m_data_semaphore;
    Semaphore m_empty_semaphore;

    bool __enqueue(int priority, const DataType& data)
    {
        if(!m_queues[priority]->enqueue(data)){
            m_empty_semaphore.post();
            return false;
        }
        m_data_semaphore.post();
        __sync_fetch_and_add(&m_size, 1);
        return true;
    }

    bool __dequeue(DataType& data)
    {
        bool result = false;
        for(unsigned i = 1; i <= m_priority_num; i++)
            if(m_queues[i] && m_queues[i]->size() && m_queues[i]->try_dequeue(data))
            {
                result = true;
                break;
            }
        if(!result){
            m_data_semaphore.post();
            return false;
        }
        m_empty_semaphore.post();
        __sync_fetch_and_sub(&m_size, 1);
        return true;
    }

    bool __test_queue(unsigned priority)
    {
         if(priority > m_priority_num){
            LOG_ERROR("CPriorityQueue::Enqueue invalid priority %d exceed max: %d\n", priority, m_priority_num);
            return false;
        }
        if(priority < 1){
            LOG_ERROR("CPriorityQueue::Enqueue invalid priority %d less than 1\n", priority);
            return false;
        }
        ElemType* pt = m_queues[priority];
        if(pt == NULL){
            pt = new ElemType(m_max_size);
            if(!__sync_bool_compare_and_swap(m_queues + priority, NULL, pt))
                delete pt;
        }
        return true;
    }
public:
    //all priority is: 1, 2, 3, ... , priority_num,  the LESS digit have HIGH prioriry
    CPriorityQueue(unsigned max_size = 1000000, int priority_num = 16): 
        m_exit(false), m_size(0), m_priority_num(priority_num), m_max_size(max_size), 
        m_queues(NULL), m_data_semaphore(0), m_empty_semaphore(max_size) 
    {
        m_queues = new ElemType* [m_priority_num + 1];
        for(unsigned i = 1; i <= m_priority_num; i++)
           m_queues[i] = new ElemType(max_size);
    }
    virtual ~CPriorityQueue()
    {
        LOG_DEBUG("Enter CPriorityQueue destruct %p\n", this);
        exit();
        for(unsigned i = 1; i <= m_priority_num; i++){
            delete m_queues[i];
            m_queues[i] = NULL;
        }
        delete[] m_queues;
        m_queues = NULL;
        LOG_DEBUG("Leave CPriorityQueue destruct %p\n", this);
    }

	size_t size()
    {
        return m_size;
    }

	bool enqueue(int priority, const DataType& data)
    {
        if(!__test_queue(priority)) 
            return false;
        if(m_exit) return false;
        m_empty_semaphore.wait();
        if(m_exit) return false;
        return __enqueue(priority, data);
    }
	bool timed_enqueue(int priority, const DataType& data, boost::posix_time::time_duration dura)
    {
        if(!__test_queue(priority)) 
            return false;
        boost::posix_time::ptime timeout = boost::get_system_time() + dura; 
        if(m_exit || !m_empty_semaphore.timed_wait(timeout) || m_exit)
            return false;
        return __enqueue(priority, data);
    }
	bool try_enqueue(int priority, const DataType& data)
    {
        if(!__test_queue(priority)) 
            return false;
        if(m_exit || m_empty_semaphore.try_wait() || m_exit) 
            return false;
        return __enqueue(priority, data);
    }

    //no wait
    bool try_dequeue(DataType& data)
    {
        if(m_exit || !m_data_semaphore.try_wait() || m_exit)
            return false;
        return __dequeue(data);
    }
	bool timed_dequeue(DataType& data, boost::posix_time::time_duration dura)
    {
        boost::posix_time::ptime timeout = boost::get_system_time() + dura;
        if(m_exit || !m_data_semaphore.timed_wait(timeout) || m_exit) 
            return false;
        return __dequeue(data);
    }
	bool dequeue(DataType& data)
    {
        if(m_exit) return false;
        m_data_semaphore.wait();
        if(m_exit) return false;
        return __dequeue(data);
    }
    void exit()
    {
        if(!__sync_bool_compare_and_swap(&m_exit, false, true)){
            LOG_INFO("CPriorityQueue::Exit %p dumplicate.\n", this);
            return;
        }
        LOG_INFO("CPriorityQueue::Exit %p ...\n", this);
        for(unsigned i = 1; i <= m_priority_num; i++)
            m_queues[i]->exit();

        do{
            m_data_semaphore.post();
            usleep(10000);
        }while(!m_data_semaphore.try_wait());

        do{
            m_empty_semaphore.post();
            usleep(10000);
        }while(!m_empty_semaphore.try_wait());
        LOG_INFO("CPriorityQueue::Exit %p end.\n", this);
    }
};

#endif
