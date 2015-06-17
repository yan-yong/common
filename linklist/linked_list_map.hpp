#ifndef __LINKED_LIST_MAP_HPP__
#define __LINKED_LIST_MAP_HPP__
#include <stdlib.h>
#include <map>
#include <boost/shared_ptr.hpp>
#include "linked_list.hpp"

template<typename K, typename T, linked_list_node_t T::*list_node>
class linked_list_map: public std::map<K, boost::shared_ptr< linked_list_t<T, list_node> > >
{
    typedef linked_list_t<T, list_node> List;
    typedef boost::shared_ptr<List> ListPtr;
    typedef std::map<K, ListPtr > Base;

private:
    ListPtr operator[](const K&);    
    
protected:
    size_t cur_cnt_;    

    ListPtr __get(const K& key) const
    {
        ListPtr p_lst;
        typename Base::const_iterator it = this->find(key);
        if(it != this->end())
            p_lst = it->second;
        return p_lst; 
    }
    ListPtr __obtain(const K& key)
    {
        typename Base::iterator it = this->find(key);
        if(it == this->end())
        {
            ListPtr val(new List());
            it = insert(typename Base::value_type(key, val)).first;
        }
        return it->second;
    }

public:
    linked_list_map(): cur_cnt_(0){}
    
    ListPtr get_list(const K& key) const
    {
        return __get(key);
    }

    bool empty(const K& key)
    {
        ListPtr p_lst = __get(key);
        if(!p_lst)
            return true;
        if(p_lst->empty())
        {
            erase(key);
            return true; 
        }
        return false; 
    }

    bool empty()
    {
        if(Base::empty())
            return true;
        for(typename Base::iterator it = Base::begin(); it != Base::end(); )
        {
            if(!it->second->empty())
                return false;
            erase(it++);
        }
        return true;
    }
    
    void clear(const K& key)
    {
        ListPtr p_lst = __get(key);
        if(p_lst)
        {
            size_t clear_size = p_lst->size();
            assert(cur_cnt_ >= clear_size);
            cur_cnt_ -= p_lst->size();
            p_lst->clear();
        }
    }
    
    void clear()
    {
        Base::clear();
        cur_cnt_ = 0;
    }    

    void add_front(const K& key, T& t)
    {
        ListPtr p_lst = __obtain(key);
        p_lst->add_front(t);
        cur_cnt_++;
    }

    void add_back(const K& key, T& t)
    {
        ListPtr p_lst = __obtain(key);
        p_lst->add_back(t);
        cur_cnt_++;
    }

    bool del(T &node)
    {
        if(List::del(node))
        {
            cur_cnt_--;
            return true;
        }
        return false;
    }

    size_t size() const
    {
        return cur_cnt_;
    }

    void get_front(K& key, T* & t)
    {
        typename Base::iterator it = Base::begin();
        while(it != Base::end() && it->second->empty())
            erase(it);
        assert(it != Base::end());
        key = it->first;
        t   = it->second->get_front(); 
    }

    void get_back(K& key, T* & t)
    {
        typename Base::iterator it = Base::rbegin();
        while(it != Base::rend() && it->second->empty())
            erase(it);
        assert(it != Base::rend());
        key = it->first;
        t   = it->second->get_back(); 
    }

    void pop_front()
    {
        typename Base::iterator it = this->begin();
        while(it != Base::end() && it->second->empty())
            erase(it);
        assert(it != Base::end());
        it->second->pop_front();
        if(it->second->empty())
            this->erase(it);
    }

    void pop_back()
    {
        typename Base::iterator it = Base::rbegin();
        while(it != Base::rend() && it->second->empty())
            erase(it);
        assert(it != Base::rend());
        it->second->pop_back();
        if(it->second->empty())
            Base::erase(it);
    }
    
    ListPtr splice()
    {
        ListPtr splice_lst(new List());
        for(typename Base::iterator it = Base::begin(); it != Base::end(); )
        {
            splice_lst->splice_front(*it->second);
            Base::erase(it++);
        }
        cur_cnt_ = 0;
        return splice_lst;
    } 
};

#endif
