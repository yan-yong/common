#ifndef __LINKED_LIST_HPP__
#define __LINKED_LIST_HPP__

#include <stdlib.h>
#include <sstream>
#include <string>
#include <assert.h>
#include "linked_node.hpp"

template <typename T, linked_list_node_t T::*list_node>
class linked_list_t
{
    linked_list_t& operator = (const linked_list_t&);
    linked_list_t(const linked_list_t&);

public:
    linked_list_t() { _head.next = _head.prev = &_head; }
    bool empty() const { return _head.next == &_head; }
    void clear() { _head.next = _head.prev = &_head; }
    linked_list_node_t& head() { return _head; }
    T* entry(linked_list_node_t &node) const { return &node == &_head ? NULL : (T*)((char*)&node - (char*)_node_offset); }

    //make sure: node.*list_node is not linked in another linklist, if so, you should del it first!!
    void add_front(T &node)
    {
        assert((node.*list_node).empty());
        _head.next->prev = &(node.*list_node);
        (node.*list_node).next = _head.next;
        (node.*list_node).prev = &_head;
        _head.next = &(node.*list_node);
    }
    
    void add_back(T& node)
    {
        assert((node.*list_node).empty());
        linked_list_node_t * p_back = _head.prev;
        linked_list_node_t * p_head = &_head;
        p_back->next = &(node.*list_node);
        (node.*list_node).prev = p_back;
        p_head->prev = &(node.*list_node);
        (node.*list_node).next = p_head;
    }

    //notice: POOL performance
    size_t size() const
    {
        size_t cur_cnt = 0;
        linked_list_node_t cur_node = _head;
        while((cur_node = next(_head)) != NULL)
            cur_cnt++;
        return cur_cnt;
    }
    
    //add node before cur
    static void add_prev(T &node, T &cur)
    {
        assert(node.*list_node.empty());
        (cur.*list_node).prev->next = &(node.*list_node);
        (node.*list_node).prev = (cur.*list_node).prev;
        (node.*list_node).next = &(cur.*list_node);
        (cur.*list_node).prev = &(node.*list_node);
    }

    static void add_next(T &node, T &cur)
    {
        assert(node.*list_node.empty());
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

    static bool del(linked_list_node_t &node)
    {
        if(node.next != &node && node.prev != &node)
        {
            node.next->prev = node.prev;
            node.prev->next = node.next;
            node.next       = &node;
            node.prev       = &node;
            return true;
        }
        return false;
    }
    
    T* get_front() const
    {
        return entry(*_head.next);
    }

    void pop_front()
    {
        del(*_head.next);
    }

    T* get_back() const
    {
        return entry(*_head.prev);
    }

    void pop_back()
    {
        del(*_head.prev);
    }

    void splice_front(linked_list_t& other)
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
        other.clear();
    }

    void splice_back(linked_list_t& other)
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
        other.clear();
    }

    T* next(T &node) const
    {
        return (node.*list_node).next == &_head ? NULL : (T*)((char*)(node.*list_node).next - (char*)_node_offset);
    }

    T* prev(T &node) const
    {
        return (node.*list_node).prev == &_head ? NULL : (T*)((char*)(node.*list_node).prev - (char*)_node_offset);
    }

    T* next(linked_list_node_t &node) const
    {
        return node.next == &_head ? NULL : (T*)((char*)node.next - (char*)_node_offset);
    }

    T* prev(linked_list_node_t &node) const
    {
        return node.prev == &_head ? NULL : (T*)((char*)node.prev - (char*)_node_offset);
    }

    std::string to_string()
    {
        std::ostringstream oss;
        if(empty())
            return oss.str();
        oss << "[";
        T* e = this->next(_head);
        while(e)
        {
            oss << *e;
            e = this->next(*e);
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
linked_list_node_t const * const linked_list_t<T, list_node>::_node_offset = &(((T *)0)->*list_node);

#endif /* __LINKED_LIST_HPP__ */

