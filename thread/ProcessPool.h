#ifndef __PROCESS_POOL_H_
#define __PROCESS_POOL_H_
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include "singleton/Singleton.h"
#include "queue/CPriorityQueue.h"

class ProcessPool 
{
    enum{
        MAX_PROCESS_NUM = 1000,
        WORK_THREAD_NUM = 10
    };

public:
    typedef boost::function<void (void)> ProcType;
    typedef boost::shared_ptr<boost::thread> ThdType;
protected:

    int m_thread_num;
    bool m_exit;
    CPriorityQueue<ProcType>      m_process;
    std::vector<ThdType>          m_threads;

    static void svc(ProcessPool* arg);

public: 
    void set_thread_num(int thread_num);
    virtual ~ProcessPool();
    void open();
	void process(int priority, ProcType taskThread, bool sync = false);
    void exit();

    //ProcessPool();
    DECLARE_SINGLETON(ProcessPool);
};

#endif
