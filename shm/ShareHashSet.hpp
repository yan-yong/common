#ifndef __SHARE_HASH_SET_HPP
#define __SHARE_HASH_SET_HPP

#include <stdint.h>
#include "ShareMem.hpp"
#include "bitmap/bit_map.h"

template<class T, class HashFunctor>
class ShareHashSet
{
public:
    typedef uint64_t HashKey;

private:
    ShareMem& share_mem_;
    unsigned *size_;
    T*  elements_;
    unsigned char* data_bitmap_;
    unsigned char* delete_bitmap_;

    void __find_set(const T& e, bool& ret, HashKey& hash_idx) const
    {
        ret = false;
        HashKey src_hash_key = HashFunctor()(e) % *size_;
        hash_idx = src_hash_key;
        long first_delete_idx = -1;
        while(true)
        {
            if(!test_bitmap((long)hash_idx, (void*)data_bitmap_))
                break;
            bool is_delete = test_bitmap((long)hash_idx, (void*)delete_bitmap_);
            if(!is_delete && !(elements_[hash_idx] < e) && !(e < elements_[hash_idx]))
            {
                ret = true;
                break;
            }
            if(is_delete && first_delete_idx < 0)
            {
                first_delete_idx = hash_idx;
            }

            if(++hash_idx >= *size_ - 1)
                hash_idx = 0;
            if(hash_idx == src_hash_key)
                break;
        }
        if(!ret && first_delete_idx >= 0)
            hash_idx = first_delete_idx;
    }

public:
    ShareHashSet(ShareMem& share_mem, unsigned n):
        share_mem_(share_mem)
    {
        size_ = share_mem_.New<unsigned>();
        if(*size_)
            assert(*size_ == n);
        else
            *size_ = n;
        unsigned bitmap_byte_num = (*size_ + 1)/8;
        data_bitmap_   = share_mem_.New<unsigned char>(bitmap_byte_num);
        assert(data_bitmap_);
        delete_bitmap_ = share_mem_.New<unsigned char>(bitmap_byte_num);
        elements_ = share_mem_.New<T>(*size_);
        assert(elements_);
    }

    bool find(const T& e) const
    {
        bool ret = false;
        HashKey hash_idx = 0;
        __find_set(e, ret, hash_idx);
        return ret;
    }

    bool insert(const T& e) const
    {
        bool ret = false;
        HashKey hash_idx = 0;
        __find_set(e, ret, hash_idx);
        if(!ret)
        {
            memcpy(elements_ + hash_idx, &e, sizeof(T));
            set_bitmap((long)hash_idx, (void*)data_bitmap_);
            clear_bitmap((long)hash_idx, (void*)delete_bitmap_);
            return true;
        }
        return false;
    }

    void update(const T& e) const
    {
        bool ret = false;
        HashKey hash_idx = 0;
        __find_set(e, ret, hash_idx);
        if(!ret)
        {
            set_bitmap((long)hash_idx, (void*)data_bitmap_);
            clear_bitmap((long)hash_idx, (void*)delete_bitmap_);
        }
        memcpy(elements_ + hash_idx, &e, sizeof(T));
    }

    bool erase(const T& e) const
    {
        bool ret = false;
        HashKey hash_idx = 0;
        __find_set(e, ret, hash_idx);
        if(!ret)
            return false;
        set_bitmap((long)hash_idx, (void*)delete_bitmap_);
        return true;
    }

    bool get_next(HashKey& hash_key, T& e) const
    {
        while(hash_key < *size_)
        {
            hash_key = find_next_bit(data_bitmap_, *size_, hash_key);
            if(hash_key >= *size_)
                return false;
            if(!test_bitmap((long)hash_key, (void*)delete_bitmap_))
            {
                memcpy(&e, elements_ + hash_key, sizeof(T));
                ++hash_key;
                return true;
            }
            ++hash_key;
        }
        return false;
    }
};

#endif
