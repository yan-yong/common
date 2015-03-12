#ifndef __THREAD_H_
#define __THREAD_H_

#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include "lock/lock.hpp"

//used to wrap boost:thread
class CThread
{
    bool m_exit;
    bool m_opened;
    bool m_loop;
    std::string m_thread_id;
    boost::shared_ptr<boost::thread> m_cur_thread;
    boost::function<void (void)> m_runtine;

protected:
    static void svc(void* arg);
    void wait();
    virtual void before_thread_exit();
    virtual void after_thread_exit();
    inline bool should_exit();

public: 
	CThread(std::string thread_id = "CThread", bool loop = true);
    virtual ~CThread();
    void exit();
    void open();
    inline const char* thread_id();
    void set_thread_id(std::string thread_id);
    void set_runtine(boost::function<void (void)> runtine);
    //do the work
    virtual void run();
};

#endif
