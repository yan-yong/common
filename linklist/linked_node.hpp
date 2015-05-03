#ifndef __LINKED_NODE_HPP
#define __LINKED_NODE_HPP

struct linked_list_node_t
{
    linked_list_node_t *next;
    linked_list_node_t *prev;

    linked_list_node_t()
    {
        next = this;
        prev = this;
    }
    bool empty()
    {
        return next == this && prev == this;
    }
};

#endif
