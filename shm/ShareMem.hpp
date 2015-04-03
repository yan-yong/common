#ifndef __SHARE_MEM_HPP
#define __SHARE_MEM_HPP

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <assert.h>
#include <sys/stat.h>

class ShareMem 
{
    char* m_pPool;
    char* cur_ptr;
    int share_mem_id;
    size_t share_mem_size;
    pthread_mutex_t lock;
public:
    ShareMem(int share_mem_key, size_t size): m_pPool(NULL), cur_ptr(NULL), share_mem_id(0), share_mem_size(size){
        bool new_create = false;
        if((share_mem_id = shmget(share_mem_key, share_mem_size, IPC_CREAT | IPC_EXCL | 0666)) < 0) {
            if((share_mem_id = shmget(share_mem_key, share_mem_size, 0666)) < 0){
                fprintf(stderr,"ShareMem: fail to get shm Key: %d, error_info: %s\n", share_mem_key, strerror(errno));
                return;
            }
            new_create = true;
            //else fprintf(stderr,"shm key:%u exist, get success.\n", share_mem_key);
        }
        //else fprintf(stderr,"create new shm succ\n");

        if ((m_pPool = (char *)shmat(share_mem_id, NULL ,0)) == (char *) -1) 
        {   
            fprintf(stderr,"ShareMem: Fail to shmat. ShmID is %d\n", share_mem_id);
            shmctl(share_mem_id, IPC_RMID, NULL);
            return; 
        }
        cur_ptr = m_pPool;
        if(new_create)
            memset(m_pPool, 0, sizeof(size));
        pthread_mutex_init(&lock, NULL); 
    }
    ~ShareMem(){
        if(m_pPool){
            shmdt(m_pPool);
            m_pPool = NULL; 
        }
    }
    int read(void* dst_ptr, size_t size){
        pthread_mutex_lock(&lock);
        if(m_pPool || cur_ptr - m_pPool + size > share_mem_size){
            pthread_mutex_unlock(&lock);
            return -1;
        }
        memcpy(dst_ptr, cur_ptr, size);
        cur_ptr += size;
        pthread_mutex_unlock(&lock);
        return 0;
    }
    int pread(void* dst_ptr, size_t size, size_t offset){
        pthread_mutex_lock(&lock);
        if(m_pPool || offset > share_mem_size || offset + size > share_mem_size){
            pthread_mutex_unlock(&lock);
            return -1;
        }
        memcpy(dst_ptr, m_pPool + offset, size);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    int write(void* src_ptr, size_t size){
        pthread_mutex_lock(&lock);
        if(m_pPool || cur_ptr - m_pPool + size > share_mem_size){ 
            pthread_mutex_unlock(&lock);
            return -1;
        }
        memcpy(cur_ptr, src_ptr, size);
        cur_ptr += size;
        pthread_mutex_unlock(&lock);
        return 0;
    }
    int pwrite(void* src_ptr, size_t size, size_t offset){
        pthread_mutex_lock(&lock);
        if(m_pPool || offset > share_mem_size || offset + size > share_mem_size){ 
            pthread_mutex_unlock(&lock);
            return -1;
        }
        memcpy(m_pPool + offset, src_ptr, size);
        pthread_mutex_unlock(&lock);
        return 0;
    }

    bool mem_ok(){
        return m_pPool != NULL;
    }

    int rewind(){
        pthread_mutex_lock(&lock);
        if(m_pPool){
            pthread_mutex_unlock(&lock);
            return -1;
        }
        cur_ptr = m_pPool;
        pthread_mutex_unlock(&lock);
        return 0;
    }

    int lseek(size_t offset){
        pthread_mutex_lock(&lock);
        if(m_pPool || offset > share_mem_size){
            pthread_mutex_unlock(&lock);
            return -1;
        }
        cur_ptr = m_pPool + offset;
        pthread_mutex_unlock(&lock);
        return 0;
    }

    void* malloc(size_t size){
        pthread_mutex_lock(&lock);
        if(!m_pPool || cur_ptr - m_pPool + size > share_mem_size)
            return NULL;
        void* ret = cur_ptr;
        cur_ptr += size; 
        pthread_mutex_unlock(&lock);
        return ret;
    }

    void* calloc(size_t size, size_t num) { 
        pthread_mutex_lock(&lock);
        if(!m_pPool || cur_ptr - m_pPool + size*num > share_mem_size){
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        void* ret = cur_ptr;
        cur_ptr += size*num;
        pthread_mutex_unlock(&lock);
        return ret; 
    }

    template<class T>
    T* New(){
        pthread_mutex_lock(&lock);
        if(!m_pPool || cur_ptr - m_pPool + sizeof(T) > share_mem_size){
        pthread_mutex_unlock(&lock);
        return NULL;
        }
        T* ret = (T*)cur_ptr;
        cur_ptr += sizeof(T);
        pthread_mutex_unlock(&lock);
        return ret;
    }

    template<class T>
    T* New(size_t num){
        pthread_mutex_lock(&lock);
        if(!m_pPool || cur_ptr - m_pPool + sizeof(T)*num > share_mem_size){
        pthread_mutex_unlock(&lock);
        return NULL;
        }
        T* ret = (T*)cur_ptr;
        cur_ptr += sizeof(T)*num;   
        pthread_mutex_unlock(&lock);
        return ret;
    }
};

#endif
