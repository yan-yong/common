#ifndef __SINGLETON_H
#define __SINGLETON_H

#include "boost/shared_ptr.hpp"

/* DECLARE_SINGLETON should be put in header file; DEFINE_SINGLETON should be put in cpp file */

#define DECLARE_SINGLETON(T) \
    protected: \
        T(); \
        T(const T&); \
        T& operator=(const T&); \
    public: \
        static T* m_instance; \
        static boost::shared_ptr<T> m_inst_guard;   \
        static boost::shared_ptr<T> Instance(){ \
            if(!m_instance) { \
                T* instance = new T();  \
                if(!__sync_bool_compare_and_swap(&m_instance, NULL, instance))  \
                    delete instance;    \
                else  \
                    m_inst_guard.reset(instance);  \
            }   \
            return m_inst_guard; \
        }  

#define DEFINE_SINGLETON(T) \
    T* T::m_instance = NULL; \
    boost::shared_ptr<T> T::m_inst_guard;

#endif
