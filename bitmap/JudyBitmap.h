#ifndef __JUDY_BITMAP_H
#define __JUDY_BITMAP_H
#include "judyarray/Judy.h"
#include <stdio.h>
#include "log/log.h"
#include "MurmurHash.h"
#include <string>
#include <string.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "Bitmap.h"

#define JUDY_BITMAP_VERSION "1.0"
#define JUDY_BITMAP_SAVE_HEADER "Judy_Bitmap_" 

#define WRITE_SIGN_INTERVAL    1024
#define SIGN_BYTES     (size_t)0xbbbbbbbbbbbbbbbb
#define BIT_MASK       0x7fffffffffffffff 
#define WORD_SIZE      sizeof(size_t) 

class JudyBitmap: public Bitmap
{
    RwMutex m_rw_lock;
    Pvoid_t m_judy;
    FILE* m_fid;
    long m_file_size;
    //notice: is items num of the file (include del item), NOT elements num of bitmap
    size_t m_cur_cnt;
    std::string m_save_file;
    bool m_need_rewrite_file;

    void operate(size_t index)
    {
        //set index should be 0 ahead
        if(!(index & ~BIT_MASK))
        {
            __set(index);
        }
        else{
            index &= BIT_MASK;
            __unset(index);
        }
    }
    int __set(size_t index)
    {
        int ret = 0;
        //fprintf(stderr, "__set %zd %p\n", index, m_judy);
        Word_t index_word = index;
        J1S(ret, m_judy, index_word);
        if (ret == JERR){
            LOG_ERROR("JudyBitmap::set error!\n");
            return -1;
        }
        return ret;
    }
    int __unset(size_t index)
    {
        int ret = 0;
        Word_t index_word = index;
        J1U(ret, m_judy, index_word);
        if (ret == JERR){
            LOG_ERROR("JudyBitmap::unset error!\n");
            return -1;
        }
        return ret;
    }
    int __get(size_t index)
    {
        int ret = 0;
        Word_t index_word = index;
        J1T(ret, m_judy, index_word);
        if (ret == JERR){
            LOG_ERROR("JudyBitmap::get error!\n");
            return -1;
        }
        return ret;
    }

    std::string file_header()
    {
        static char header[1024];
        snprintf(header, 1024, "%s%s\n", JUDY_BITMAP_SAVE_HEADER, JUDY_BITMAP_VERSION);
        return header; 
    }
    int write(const void* ptr, size_t len, bool failed_exit = true)
    {
        int ret = 0;
        if(fwrite(ptr, 1, len, m_fid) != len){
            LOG_ERROR("Error JudyBitmap::write: cannot save to %s\n", m_save_file.c_str());
            if(failed_exit)
                exit(1);
            ret = -1;
        }
        return ret;
    }
    int write_element(const void* ptr, bool failed_exit = true)
    {
        //fprintf(stderr, "write %zd offset %zd %p\n", *(size_t*)ptr, m_judy, ftell(m_fid));
        if(write(ptr, WORD_SIZE, failed_exit) < 0)
            return -1;
        size_t sign_bytes = SIGN_BYTES;
        if(m_cur_cnt % WRITE_SIGN_INTERVAL == 0 && write(&sign_bytes, WORD_SIZE, failed_exit) < 0)
            return -1;
        return 0;
    }
    void open_file()
    {
        m_fid = fopen(m_save_file.c_str(), "a+");
        if(!m_fid){
            LOG_ERROR("JudyBitmap::open_file: cannot open file %s\n", m_save_file.c_str());
            exit(1);
        }
        std::string header = file_header();
        write(header.c_str(), header.size());
    }

    size_t __size()
    {
        Word_t present_num = 0;
        J1C(present_num, m_judy, 0, -1);
        return present_num;
    }

    void rewrite_file();
    int load_file(std::string file_name);

public:

    JudyBitmap(): m_judy((Pvoid_t)NULL), m_fid(NULL), m_file_size(0), m_cur_cnt(0), m_need_rewrite_file(false)
    {
    }

    ~JudyBitmap();
    
    virtual int initialize(int order, std::string file_name = "");
   
    virtual int get(size_t digit)
    {
        ReadLocker lock(m_rw_lock);
        return __get(digit);
    }
    virtual int set(size_t digit)
    {
        WriteLocker lock(m_rw_lock);
        int ret = __set(digit);
        if(ret > 0){
            m_cur_cnt++;
            if(write_element(&digit, false) < 0)
                return -1;
        }
        return ret;
    }
    virtual int get(const std::string& data)
    {
        ReadLocker lock(m_rw_lock);
        size_t hash_index = 0;
        MurmurHash_x64_64(data.c_str(), data.size(), &hash_index);
        hash_index &= BIT_MASK;
        return __get(hash_index);
    }
    virtual int set(const std::string& data)
    {
        WriteLocker lock(m_rw_lock);
        if(!m_fid){
            LOG_ERROR("Error JudyBitmap::set: cannot save to %s\n", m_save_file.c_str());
            return -1;
        }
        size_t hash_index = 0;
        MurmurHash_x64_64(data.c_str(), data.size(), &hash_index);
        hash_index &= BIT_MASK;
        int ret = __set(hash_index);
        if(ret > 0){
            m_cur_cnt++;
            if(write_element(&hash_index, false) < 0)
                return -1;
        }
        return ret;
    }
    virtual int unset(size_t digit)
    {
         WriteLocker lock(m_rw_lock);
        if(!m_fid){
            LOG_ERROR("Error JudyBitmap::set: cannot save to %s\n", m_save_file.c_str());
            return -1;
        }
        int ret = __unset(digit);
        if(ret > 0){
            m_cur_cnt++;
            if(write_element(&digit, false) < 0)
                return -1;
        }
        return ret;
    }
    virtual int unset(const std::string& data)
    {
        WriteLocker lock(m_rw_lock);
        if(!m_fid){
            LOG_ERROR("Error JudyBitmap::set: cannot save to %s\n", m_save_file.c_str());
            return -1;
        }
        size_t hash_index = 0;
        MurmurHash_x64_64(data.c_str(), data.size(), &hash_index);
        hash_index &= BIT_MASK;
        int ret = __unset(hash_index);
        if(ret > 0){
            m_cur_cnt++;
            hash_index |= ~BIT_MASK;
            if(write_element(&hash_index, false) < 0)
                return -1;
        }
        return ret;
    }
    virtual size_t size()
    {
        ReadLocker lock(m_rw_lock);
        return __size();
    }
    virtual size_t bytes()
    {
        ReadLocker lock(m_rw_lock);
        Word_t byte_num = 0;
        J1MU(byte_num, m_judy);
        return (size_t)byte_num;
    }
};

#endif
