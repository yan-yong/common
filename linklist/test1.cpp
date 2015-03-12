#ifndef __LINKED_LIST_MAP_HPP__
#define __LINKED_LIST_MAP_HPP__
#include <stdlib.h>
#include <map>
#include <stdio.h>

class Base
{
public:
    void fun(int i)
    {
        printf("base fun.\n");
    }
};

class Child: public Base
{
public:
    void fun()
    {
        printf("child fun.\n");
    }
};

int main()
{
    Child child;
    child.fun(2);
}

#endif
