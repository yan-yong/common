#ifndef __SHARED_LINKED_LIST_HPP__
#define __SHARED_LINKED_LIST_HPP__

#include <stdlib.h>
#include <sstream>
#include <string>
#include <assert.h>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include "linked_node.hpp"

template <typename T, linked_list_node_t T::*list_node>
class shared_linked_list_t
{
    typedef boost::shared_ptr<T> shared_ptr_t;
    typedef boost::unordered_map<T*, shared_ptr_t> shared_map_t;
    shared_map_t instances_;

    T* __entry(linked_list_node_t &node) const 
    { 
        return &node == &_head ? NULL : (T*)((char*)&node - (char*)_node_offset); 
    }

    shared_ptr_t __shared_ptr(T* raw_ptr)
    {
        if(!raw_ptr)
            return shared_ptr_t();
        typename shared_map_t::iterator it = instances_.find(raw_ptr);
        assert(it != instances_.end());
        return it->second;
    }

public:
    shared_linked_list_t() { _head.next = _head.prev = &_head; }
    shared_linked_list_t& operator =(const shared_linked_list_t & t) 
    {
        _head = t._head; 
        return *this; 
    }
    bool empty() const { return _head.next == &_head; }
    void clear() { _head.next = _head.prev = &_head; instances_.clear(); }
    linked_list_node_t& head() { return _head; }

    shared_ptr_t entry(linked_list_node_t &node)
    {
        return __shared_ptr(__entry(node));
    }

    //make sure: node.*list_node is not linked in another linklist, if so, you should del it first!!
    void add_front(shared_ptr_t node)
    {
        T* raw_ptr = node.get();
        assert((raw_ptr->*list_node).empty());
        _head.next->prev = &(raw_ptr->*list_node);
        (raw_ptr->*list_node).next = _head.next;
        (raw_ptr->*list_node).prev = &_head;
        _head.next = &(raw_ptr->*list_node);
        instances_[raw_ptr] = node;
    }
    
    void add_back(shared_ptr_t node)
    {
        T* raw_ptr = node.get();
        assert((raw_ptr->*list_node).empty());
        linked_list_node_t * p_back = _head.prev;
        linked_list_node_t * p_head = &_head;
        p_back->next = &(raw_ptr->*list_node);
        (raw_ptr->*list_node).prev = p_back;
        p_head->prev = &(raw_ptr->*list_node);
        (raw_ptr->*list_node).next = p_head;
        instances_[raw_ptr]  = node;
    }

    size_t size() const
    {
        return instances_.size();
    }
    
    //add node before cur
    void add_prev(shared_ptr_t node, shared_ptr_t cur)
    {
        assert(node->*list_node.empty());
        (cur.*list_node).prev->next = &(node->*list_node);
        (node->*list_node).prev = (cur->*list_node).prev;
        (node->*list_node).next = &(cur->*list_node);
        (cur->*list_node).prev = &(node->*list_node);
        instances_[node.get()] = node;
    }

    void add_next(shared_ptr_t node, shared_ptr_t cur)
    {
        assert(node->*list_node.empty());
        (cur->*list_node).next->prev = &(node->*list_node);
        (node->*list_node).next = (cur->*list_node).next;
        (node->*list_node).prev = &(cur->*list_node);
        (cur->*list_node).next = &(node->*list_node);
        instances_[node.get()] = node;
    }

    bool del(shared_ptr_t node)
    {
        if((node->*list_node).next != &(node->*list_node) && (node->*list_node).prev != &(node->*list_node))
        {
            (node->*list_node).next->prev = (node->*list_node).prev;
            (node->*list_node).prev->next = (node->*list_node).next;
            (node->*list_node).next = &(node->*list_node);
            (node->*list_node).prev = &(node->*list_node);
            instances_.erase(node.get());
            return true;
        }
        return false;
    }

    bool del(linked_list_node_t &node)
    {
        if(node.next != &node && node.prev != &node)
        {
            T* raw_ptr = __entry(node);
            node.next->prev = node.prev;
            node.prev->next = node.next;
            node.next       = &node;
            node.prev       = &node;
            instances_.erase(raw_ptr);
            return true;
        }
        return false;
    }
    
    shared_ptr_t get_front()
    {
        return entry(*_head.next);
    }

    void pop_front()
    {
        assert(del(*_head.next));
    }

    shared_ptr_t get_back()
    {
        return entry(*_head.prev);
    }

    void pop_back()
    {
        assert(del(*_head.prev));        
    }

    void splice_front(shared_linked_list_t& other)
    {
        if(other.empty())
            return;
        linked_list_node_t* other_first_node = other._head.next;
        linked_list_node_t* other_back_node  = other._head.prev;
        linked_list_node_t* cur_first_node   = _head.next;
        other_back_node->next     = cur_first_node;
        cur_first_node->prev      = other_back_node;
        _head.next                = other_first_node;
        other_first_node->prev    = &_head;
        instances_.insert(other.instances_.begin(), other.instances_.end());
        other.clear();
    }

    void splice_back(shared_linked_list_t& other)
    {
        if(other.empty())
            return;
        linked_list_node_t* other_first_node = other._head.next;
        linked_list_node_t* other_back_node = other._head.prev;
        linked_list_node_t* cur_back_node   = _head.prev;
        cur_back_node->next    = other_first_node;
        other_first_node->prev = cur_back_node;
        other_back_node->next  = &_head;
        _head.prev             = other_back_node; 
        instances_.insert(other.instances_.begin(), other.instances_.end());
        other.clear();
    }

    shared_ptr_t next(shared_ptr_t node)
    {
        T* raw_ptr = (node.get()->*list_node).next == &_head ? 
            NULL : (T*)((char*)(node.get()->*list_node).next - (char*)_node_offset);
        return __shared_ptr(raw_ptr);
    }

    shared_ptr_t prev(shared_ptr_t node)
    {
        T* raw_ptr = (node.get()->*list_node).prev == &_head ? 
            NULL : (T*)((char*)(node.get()->*list_node).prev - (char*)_node_offset);
        return __shared_ptr(raw_ptr);
    }

    shared_ptr_t next(linked_list_node_t &node)
    {
        T* raw_ptr = node.next == &_head ? NULL : (T*)((char*)node.next - (char*)_node_offset);
        return __shared_ptr(raw_ptr);
    }

    shared_ptr_t prev(linked_list_node_t &node)
    {
        T* raw_ptr = node.prev == &_head ? NULL : (T*)((char*)node.prev - (char*)_node_offset);
        return __shared_ptr(raw_ptr);
    }

    std::string to_string()
    {
        std::ostringstream oss;
        if(empty())
            return oss.str();
        oss << "[";
        shared_ptr_t e = this->next(_head);
        while(e)
        {
            oss << *e;
            e = this->next(e);
            if(e)
                oss << ",";
        }
        oss << "]";
        return oss.str();
    }
    
#if 0
    //replace static method with object method because of recording element count
    static void add_prev(T &node, T &cur)
    {
        (cur.*list_node).prev->next = &(node.*list_node);
        (node.*list_node).prev = (cur.*list_node).prev;
        (node.*list_node).next = &(cur.*list_node);
        (cur.*list_node).prev = &(node.*list_node);
    }
    static void add_next(T &node, T &cur)
    {
        (cur.*list_node).next->prev = &(node.*list_node);
        (node.*list_node).next = (cur.*list_node).next;
        (node.*list_node).prev = &(cur.*list_node);
        (cur.*list_node).next = &(node.*list_node);
    }
    static bool del(T &node)
    {
        if((node.*list_node).next != &(node.*list_node) && (node.*list_node).prev != &(node.*list_node))
        {
            (node.*list_node).next->prev = (node.*list_node).prev;
            (node.*list_node).prev->next = (node.*list_node).next;
            (node.*list_node).next = &(node.*list_node);
            (node.*list_node).prev = &(node.*list_node);
            return true;
        }
        return false;
    }
#endif

protected:
    static linked_list_node_t const * const _node_offset;
    linked_list_node_t _head;
};

template <typename T, linked_list_node_t T::*list_node>
linked_list_node_t const * const shared_linked_list_t<T, list_node>::_node_offset = &(((T *)0)->*list_node);

#endif /* __LINKED_LIST_HPP__ */

