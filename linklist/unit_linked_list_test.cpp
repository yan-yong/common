#include <stdio.h>
#include <ostream>
#include "linked_list.hpp"
#include "shared_linked_list.hpp"
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
    typedef boost::shared_ptr<Obj> shared_ptr_t;
    shared_linked_list_t<Obj, &Obj::node_> share_lst;
    {   
        shared_ptr_t obj1(new Obj(1));
        shared_ptr_t obj2(new Obj(2));
        shared_ptr_t obj3(new Obj(3));
        shared_ptr_t obj4(new Obj(4));

        share_lst.add_back(obj2);
        share_lst.add_back(obj1);
        share_lst.add_front(obj3);
        share_lst.add_back(obj4);
    }   
    printf("%s\n", share_lst.to_string().c_str());
    shared_ptr_t ptr = share_lst.get_front();
    std::cout<<*ptr<<std::endl;
    share_lst.pop_front();
    printf("%s\n", share_lst.to_string().c_str());
    share_lst.pop_back();
    printf("%s\n", share_lst.to_string().c_str());

    linked_list_t<Obj, &Obj::node_> shared_lst;
    Obj obj1(1);
    Obj obj2(2);
    Obj obj3(3);
    Obj obj4(4);
    Obj obj11(11);
    Obj obj22(22);
    
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
