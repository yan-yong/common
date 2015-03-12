#ifndef __LOCK_HPP
#define __LOCK_HPP

#include <boost/thread/mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/thread/shared_mutex.hpp>

typedef boost::mutex Mutex;
typedef boost::mutex::scoped_lock MutexGuard;
typedef boost::interprocess::interprocess_semaphore Semaphore;

typedef boost::shared_mutex RwLock;
typedef boost::shared_lock<RwLock> ReadGuard;
typedef boost::unique_lock<RwLock> WriteGuard;

class SpinLock
{
    static const int TOKEN_FREE = 0;
    static const int TOKEN_LOCK = 1;
    volatile int lock_val_;

    SpinLock(const SpinLock&);
    SpinLock& operator = (const SpinLock&);

public:
    SpinLock(): lock_val_(TOKEN_FREE)
    {
    }
    virtual void Lock()
    {
        while(!__sync_bool_compare_and_swap(&lock_val_, TOKEN_FREE, TOKEN_LOCK));
    }
    virtual void Unlock()
    {
        __sync_bool_compare_and_swap(&lock_val_, TOKEN_LOCK, TOKEN_FREE);
    }
};

class SpinGuard
{
    SpinLock& lock_;
    SpinGuard(SpinGuard&);
    SpinGuard& operator = (const SpinGuard&);

public:
    SpinGuard(SpinLock& lock): lock_(lock)
    {
        if(&lock_)
            lock_.Lock();
    }
    ~SpinGuard()
    {
        if(&lock_)
            lock_.Unlock();
    }
};

#endif
