#include "ProcessPool.h"

DEFINE_SINGLETON(ProcessPool);
ProcessPool::ProcessPool(): 
    m_thread_num(WORK_THREAD_NUM), 
    m_exit(false),
    m_process(MAX_PROCESS_NUM)
{
}

void ProcessPool::set_thread_num(int thread_num)
{
    m_thread_num = thread_num;  
}

void ProcessPool::open()
{
    for(int i = 0; i < m_thread_num; i++){
        ThdType thd(new boost::thread(ProcessPool::svc, this));
        m_threads.push_back(thd);
    }
}

ProcessPool::~ProcessPool()
{
    exit();
}

void ProcessPool::process(int priority, ProcType taskThread, bool sync)
{
    if(sync)
        return taskThread();
    m_process.enqueue(priority, taskThread);
}

void ProcessPool::svc(ProcessPool* arg)
{
    ProcType proc;
    while(!arg->m_exit && arg->m_process.dequeue(proc)){
        proc();
    } 
}

void ProcessPool::exit()
{
    if(!__sync_bool_compare_and_swap(&m_exit, false, true))
        return;
    LOG_DEBUG("ProcessPool exiting ...\n");
    m_process.exit();
    for(unsigned i = 0; i < m_threads.size(); i++)
        m_threads[i]->join();
    LOG_DEBUG("ProcessPool exit\n");
}
