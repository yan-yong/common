#include <stdio.h>
#include <ostream>
#include "linked_list.hpp"
#include "linked_list_map.hpp"
using namespace std;

struct Obj
{
    linked_list_node_t node_;
    int  e_;
    Obj(int e): e_(e){}
};

ostream& operator << (ostream & os, const Obj& obj) 
{
    return os << obj.e_;
}

int main()
{
    linked_list_t<Obj, &Obj::node_> lst;
    Obj obj1(1);
    Obj obj2(2);
    Obj obj3(3);
    Obj obj4(4);
    Obj obj11(11);
    Obj obj22(22);

    /*
    lst.add_back(obj2);
    lst.add_back(obj1);
    lst.add_front(obj3);
    lst.add_back(obj4);
    
    linked_list_t<Obj, &Obj::node_> lst1;
    lst1.add_front(obj11);
    lst1.add_front(obj22);
    lst.splice_back(lst1);
    linked_list_t<Obj, &Obj::node_> lst2;
    lst.splice_front(lst2);
    lst.pop_back();
    */
    
    linked_list_map<int, Obj, &Obj::node_> tmp_map;
    tmp_map.add_back(2, obj2);
    printf("%s\n", tmp_map.get_list(2)->to_string().c_str());

    tmp_map.add_back(22, obj22);
    tmp_map.add_back(1, obj1);
    tmp_map.add_back(1, obj11);
    tmp_map.del(obj11);
    while(!tmp_map.empty())
    {
        int key;
        Obj * p_obj;
        tmp_map.get_front(key, p_obj);
        printf("%d %d\n", key, p_obj->e_);
        tmp_map.pop_front();
    }
}
