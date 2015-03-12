#ifndef __DATA_POOL_H
#define __DATA_POOL_H

#include "queue/CQueue.h"
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread/thread.hpp> 

template<typename DataType, typename QueueType = CQueue<DataType, std::queue<DataType> > >
class DataPool
{
public:
    typedef boost::shared_ptr<boost::thread> ThdType;     
    enum{
        MAX_DATA_NUM = 1000, 
        WORK_THREAD_NUM = 10
    };

protected:
    int m_thread_num;
    bool m_exit;
    std::vector<ThdType> m_threads;
    QueueType m_queue;
    boost::function<void (void)> m_runtine;

    static void svc(void* arg)
    {
        DataPool* cur_obj = (DataPool*)arg;
        if(cur_obj->m_runtine)
            return (cur_obj->m_runtine)();
        cur_obj->run();
    }

public:
    DataPool(): m_thread_num(WORK_THREAD_NUM), m_exit(false), m_queue(MAX_DATA_NUM)
    {
    }
    int initialize(int thread_num, int max_queue_num)
    {
        m_thread_num = thread_num;
        m_queue.set_max_size(max_queue_num);
        return 0;
    }
    void open()
    {
        for(int i = 0; i < m_thread_num; i++)
            m_threads.push_back(ThdType(new boost::thread(DataPool::svc, this)));
    }
    void exit()
    {
        if(!__sync_bool_compare_and_swap(&m_exit, false, true))
            return;
        LOG_DEBUG("DataPool exiting ...\n");
        m_queue.exit();
        for(unsigned i = 0; i < m_threads.size(); i++)
            m_threads[i]->join();
        LOG_DEBUG("DataPool exited.\n");
    }
    void set_thread_num(int thread_num)
    {
        m_thread_num = thread_num;
    }
    //you can provide you owner definition by set_runtine or rewrite run
    void set_runtine(boost::function<void (void)> runtine)
    {
        m_runtine = runtine;
    }
    virtual void handle_data(DataType& data)
    {  
    }
    virtual void run()
    {
        DataType data;
        while(m_queue.dequeue(data))
            handle_data(data); 
    }
    inline bool enqueue(const DataType& data)
    {
        return m_queue.enqueue(data); 
    }
	inline bool try_enqueue(const DataType& data)
    {
        return m_queue.try_enqueue(data);  
    }
	inline bool timed_enqueue(const DataType& data, boost::posix_time::time_duration dura)
    {
        return m_queue.timed_enqueue(data);  
    }
    bool dequeue(DataType& data)
    {
        return m_queue.dequeue(data);
    }
    bool try_dequeue(DataType& data)
    {
        return m_queue.try_dequeue(data);
    }
    bool timed_dequeue(DataType& data, boost::posix_time::time_duration dura)
    {
        return m_queue.timed_dequeue(data, dura);
    }
};
#endif
