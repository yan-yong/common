#include <boost/thread/thread.hpp> 
#include "Thread.h"
#include "log/log.h"

using namespace std;

CThread::CThread(std::string thread_id, bool loop) :
    m_exit(false), m_opened(false), 
    m_loop(loop), m_thread_id(thread_id)
{
}

CThread::~CThread()
{
    exit();
}

void CThread::open(){
     if(!__sync_bool_compare_and_swap(&m_opened, false, true))
         return;
     m_cur_thread.reset(new boost::thread(CThread::svc, this));
}

void CThread::exit()
{
    if(!__sync_bool_compare_and_swap(&m_exit, false, true))
        return;
    before_thread_exit();
    wait();
    after_thread_exit();
}

void CThread::before_thread_exit()
{
}

void CThread::after_thread_exit()
{
}

void CThread::wait()
{
    m_cur_thread->join();
}

inline bool CThread::should_exit()
{
    return m_exit;
}

void CThread::svc(void* arg)
{
    CThread* cur_obj = (CThread*)arg;
    LOG_INFO("Thread %s start.\n", cur_obj->m_thread_id.c_str());
    do
    {
        if(cur_obj->m_runtine)
            (cur_obj->m_runtine)();
        else
            cur_obj->run(); 
    }while(!cur_obj->m_exit && cur_obj->m_loop);

    LOG_INFO("Thread %s end.\n", cur_obj->m_thread_id.c_str());
}

void CThread::set_thread_id(std::string thread_id)
{
    m_thread_id = thread_id;
}

void CThread::set_runtine(boost::function<void (void)> runtine)
{
    m_runtine = runtine;
}

inline const char* CThread::thread_id()
{
    return m_thread_id.c_str();
}

void CThread::run()
{
}
