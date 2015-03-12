#ifndef __DENSE_BITMAP_H
#define __DENSE_BITMAP_H
#include "Bitmap.h"
#include <boost/atomic/atomic.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "bit_map.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "MurmurHash.h"
#include "log/log.h"

#define DENSE_BITMAP_VERSION "1.0"
#define DENSE_BITMAP_SAVE_HEADER "Dense_Bitmap_" 
#define WRITE_MERGE_RAGE   0.5

class DenseBitmap: public Bitmap{
    enum{
        DEFAULT_MIN_SAVE_TIME = 60,
        DEFAULT_MAX_SAVE_TIME = 120,
        MUTEX_NUM = 256
    };

    int m_order;
    std::string m_save_file;
    int  m_fid;
    void*  m_mem;
    //each bit represent 512 bytes
    void*  m_block_state;
    time_t m_min_save_interval;
    time_t m_max_save_interval;
    std::string m_header;
    size_t m_bitmap_size;
    size_t m_block_size;
    bool   m_exit;
    pthread_t m_save_thread_id;
    bool   m_saving;
    time_t m_last_save_time;
    bool   m_write_failed;
    pthread_mutex_t m_block_locks[MUTEX_NUM];
    volatile size_t m_element_cnt;
    bool   m_auto_save;

    std::string __file_header()
    {
        return std::string(DENSE_BITMAP_SAVE_HEADER) + DENSE_BITMAP_VERSION + "\n"; 
    }
    size_t __io_merge_threadhold(size_t bytes_num)
    {
        switch(bytes_num){
            case 16:
                return 64;
            case 8:
                return 32;
            case 4:
                return 16;
            case 2:
                return 8;
            case 1:
                return 4;
            case 0:
                assert(false);
        }
        return 0;
    }
    bool __read_header()
    {
        assert(m_fid > 0);
        char buf[1024] = {0};
        ssize_t ret = pread(m_fid, buf, m_header.size(), 0);
        if(ret != (ssize_t)m_header.size()){
            LOG_ERROR("[Bitmap] read %s error: %s\n", m_save_file.c_str(), strerror(errno));
            return false;
        }
        if(m_header != buf){
            LOG_ERROR("[Bitmap] file %s header not compare: %s\n", m_save_file.c_str(), buf);
            return false;
        }
        return true;
    }
    bool __read_mem(off_t byte_offset, size_t length)
    {
        ssize_t ret = pread(m_fid, (char*)m_mem + byte_offset, length, m_header.size() + byte_offset);
        if(ret != (ssize_t)length){
            LOG_ERROR("[Bitmap] read %s error: %s\n", m_save_file.c_str(), strerror(errno));
            return false;
        }
        return true; 
    }
    bool __write_header()
    {
        assert(m_fid > 0);
        ssize_t ret = pwrite(m_fid, m_header.c_str(), m_header.size(), 0);
        if(ret != (ssize_t)m_header.size()){
            LOG_ERROR("[Bitmap] write %s error: %s\n", m_save_file.c_str(), strerror(errno));
            return false;
        }
        return true;
    }
    bool __write_mem(off_t byte_offset, size_t length)
    {
        assert(m_fid > 0 && byte_offset < (ssize_t)m_bitmap_size && byte_offset >= 0);
        ssize_t ret = pwrite(m_fid, (char*)m_mem + byte_offset, length, m_header.size() + byte_offset);
        if(ret != (ssize_t)length){
            LOG_ERROR("[Bitmap] write %s error: %s\n", m_save_file.c_str(), strerror(errno));
            return false;
        }
        return true;
    }
    void __check_save()
    {
        if(m_saving || m_write_failed || m_fid < 0)
            return;
        time_t save_interval = rand() % (m_max_save_interval - m_min_save_interval) + m_min_save_interval;
        time_t cur_time = time(NULL);
        if(cur_time - m_last_save_time > save_interval && m_saving) 
        {
            m_last_save_time = cur_time;
            pthread_create(&m_save_thread_id, NULL, save_routine, this);
        }
    }
    size_t __get_lock_id(size_t block_bit_offset)
    {
        return (block_bit_offset / (8 * MUTEX_NUM)) % MUTEX_NUM; 
    }
    //proccessing at least one byte for m_block_state
    bool __write_block(size_t& block_bit_offset, size_t& block_bit_size)
    {
        assert(block_bit_offset < block_bit_size && block_bit_offset <= m_block_size * 8);
        size_t write_mem_beg  = block_bit_offset*512;
        size_t write_byte_num = 512;
        size_t cur_lock_id  = __get_lock_id(block_bit_offset);
        //current block memory address
        void*  cur_block_mem  = (char*)m_block_state + block_bit_offset/8;
        //memory byte beginning bit for block_bit_offset 
        size_t mem_block_bit_offset = block_bit_offset - block_bit_offset % 8;
        pthread_mutex_lock(&m_block_locks[cur_lock_id]);
        //try io write merge
        size_t byte_num = m_block_size / MUTEX_NUM;
        while(byte_num >= 1 && mem_block_bit_offset + byte_num < m_bitmap_size){
            unsigned long bit_cnt = count_word_bit(cur_block_mem, byte_num);
            if(bit_cnt < WRITE_MERGE_RAGE*byte_num*8) { 
                byte_num = bit_cnt / (WRITE_MERGE_RAGE*8);
                continue;
            }
            //skip byte which not in the same lock 
            if(cur_lock_id != __get_lock_id(mem_block_bit_offset + byte_num*8)){
                byte_num /= 2;
                continue;
            }
            write_byte_num   = 512 * (byte_num*8 - block_bit_offset % 8);
            block_bit_offset = mem_block_bit_offset + byte_num*8;
            memset(cur_block_mem, 0, byte_num);
            block_bit_size -= byte_num*8;
            pthread_mutex_unlock(&m_block_locks[cur_lock_id]);
            LOG_DEBUG("Write offset:%zd %zd bytes.\n", write_mem_beg, write_byte_num);
            return __write_mem(write_mem_beg, write_byte_num);
        }
        //record bit which is seted in the current byte
        uint8_t byte_val = *(uint8_t*)cur_block_mem;
        size_t bit_record[64] = {0};
        int record_cnt = 0;
        for(int i = block_bit_offset % 8; i < 8; i++)
            if((byte_val >> i) & 0x01)
                bit_record[record_cnt++] = mem_block_bit_offset + i;
        memset(cur_block_mem, 0, 1);
        pthread_mutex_unlock(&m_block_locks[cur_lock_id]);
        block_bit_offset = mem_block_bit_offset + 8;
        block_bit_size  -= 8;
        //perform io write
        for(int i = 0; i < record_cnt; i++) {
            write_mem_beg = bit_record[i]*512;
            LOG_DEBUG("Write offset:%zd 512 bytes.\n", write_mem_beg);
            if(!__write_mem(write_mem_beg, write_byte_num))
                return false;
        }
        return true;
    }

public:
    static void* save_routine(void* arg)
    {
        DenseBitmap * obj = (DenseBitmap*) arg;
        if(obj->m_save_file.empty())
            return NULL;
        LOG_INFO("[Bitmap] %s check save ...\n", obj->m_save_file.c_str());
        if(!__sync_bool_compare_and_swap(&obj->m_saving, false, true)) {
            LOG_INFO("[Bitmap] %s prevent concurent saving.\n", obj->m_save_file.c_str());
            return NULL;
        }
        size_t block_bit_size   = obj->m_block_size * 8;
        size_t block_bit_offset = 0;
        while(true){
            block_bit_offset = find_next_bit(obj->m_block_state, block_bit_size, block_bit_offset);
            if(block_bit_offset == block_bit_size)
                break;
            if(!obj->__write_block(block_bit_offset, block_bit_size)) {
                LOG_ERROR("[Bitmap] write file %s failed.", obj->m_save_file.c_str());
                __sync_bool_compare_and_swap(&(obj->m_write_failed), false, true);
                break;
            }
        }
        __sync_bool_compare_and_swap(&(obj->m_saving), true, false);
        obj->m_save_thread_id = 0;
        LOG_INFO("[Bitmap] %s save end.\n", obj->m_save_file.c_str());
        return NULL;
    }

    DenseBitmap(): m_order(0), m_fid(-1), m_mem(NULL), m_block_state(NULL), 
        m_bitmap_size(0), m_block_size(0), m_exit(false), 
        m_save_thread_id(0), m_saving(false), 
        m_write_failed(false), m_element_cnt(0), m_auto_save(false)
    {
        m_header = __file_header();
        m_min_save_interval = DEFAULT_MIN_SAVE_TIME;
        m_max_save_interval = DEFAULT_MAX_SAVE_TIME;
        m_last_save_time = time(NULL);
    }
    ~DenseBitmap()
    {
        if(m_fid > 0) {
            close(m_fid);
            m_fid = -1;
        }
        if(m_mem){
            free(m_mem);
            m_mem = NULL;
        }
        if(m_block_state) {
            free(m_block_state);
            m_block_state = NULL;
        }
        for(int i = 0; m_fid > 0 && i < MUTEX_NUM; i++)
            pthread_mutex_destroy(m_block_locks+i);
    }
    virtual int initialize(int order, std::string file_name = "");

    void set_auto_save(bool auto_save)
    {
        m_auto_save = auto_save;
    }
    void set_save_interval(time_t min_save_interval, time_t max_save_interval)
    {
        m_min_save_interval = min_save_interval;
        m_max_save_interval = max_save_interval;
    }
    void exit()
    {
        if(!__sync_bool_compare_and_swap(&m_exit, false, true)){
            LOG_ERROR("Dumplicate exit operation.\n");
            return;
        }
        if(m_save_thread_id != 0)
            pthread_join(m_save_thread_id, NULL);
        save_routine(this);
    }
    virtual int get(size_t offset)
    {
        assert(m_mem && offset < m_bitmap_size*8);
        int ret = test_bitmap(offset, m_mem);
        if(m_auto_save)
            __check_save();
        return ret;
    }
    //return 0: not exist, or 1: exist
    virtual int get(const std::string& data)
    {
        size_t hash_val = 0;
        MurmurHash_x64_64(data.c_str(), data.size(), &hash_val);
        hash_val = (hash_val << (64 - m_order)) >> (64 - m_order);
        return get(hash_val);
    }
    //return -1: failed, 0: success, 1: dumplicate
    virtual int set(size_t offset)
    {
        assert(m_mem && offset < m_bitmap_size*8);
        if(m_write_failed){
            LOG_ERROR("[Bitmap] prevent write bitmap for failed file writing %s.\n", m_save_file.c_str());
            return -1;
        }
        size_t block_id = offset / (512*8);
        int ret = set_bitmap(offset, m_mem);
        if(ret == 0) 
        {
            m_element_cnt++;
            if(m_fid > 0)
            {
                size_t cur_lock_id  = __get_lock_id(offset);
                pthread_mutex_lock(m_block_locks + cur_lock_id);
                set_bitmap(block_id, m_block_state);
                pthread_mutex_unlock(m_block_locks + cur_lock_id);
            }
        }
        if(m_auto_save)
            __check_save();
        return ret;
    }
    virtual int set(const std::string& data)
    {
        size_t hash_val = 0;
        MurmurHash_x64_64(data.c_str(), data.size(), &hash_val);
        hash_val = (hash_val << (64 - m_order)) >> (64 - m_order);
        return set(hash_val);
    }
    //return 0: success, -1: failed
    virtual int unset(size_t offset)
    {
        assert(m_mem && offset < m_bitmap_size*8);
        if(m_write_failed){
            LOG_ERROR("[Bitmap] prevent write bitmap for failed file writing %s.\n", m_save_file.c_str());
            return -1;
        }
        size_t block_id = offset / (512*8);
        if(clear_bitmap(offset, m_mem) == 0) {
            m_element_cnt--;
            if(m_fid > 0){
                size_t cur_lock_id  = __get_lock_id(offset);
                pthread_mutex_lock(m_block_locks + cur_lock_id);
                set_bitmap(block_id, m_block_state);
                pthread_mutex_unlock(m_block_locks + cur_lock_id);
            }
        }
        if(m_auto_save)
            __check_save();
        return 0;
    }
    virtual int unset(const std::string& data)
    {
        size_t hash_val = 0;
        MurmurHash_x64_64(data.c_str(), data.size(), &hash_val);
        hash_val = (hash_val << (64 - m_order)) >> (64 - m_order);
        return unset(hash_val);  
    }
    virtual size_t size()
    {
        return m_element_cnt;                  
    }
    virtual size_t bytes()
    {
        return m_bitmap_size; 
    }
};
#endif
