#include "ShareHashSet.hpp"
#include "utility/murmur_hash.h"
#include <unistd.h>

struct Obj
{
    char buf[10];

    bool operator < (const Obj& other) const
    {
        return strncmp(buf, other.buf, 10) < 0;
    }

    Obj()
    {
    }
    
    Obj(const char* str)
    {
        strncpy(buf, str, 10);
    }
};

class HashFunctor
{
public:
    uint64_t operator() (const Obj& obj)
    {
        uint64_t val = 0;
        MurmurHash_x64_64(obj.buf, 10, &val);
        return val;
    }
};

int main()
{
    ShareMem share_mem(12345, 10000000);  
    ShareHashSet<Obj, HashFunctor> share_set(share_mem, 100000);

    share_set.insert("11111");
    share_set.insert("22222");
    share_set.insert("33333");
    share_set.insert("33333");
    share_set.erase("11111");
    share_set.insert("11111");
    share_set.insert("33333");
    share_set.insert("22222");
    share_set.insert("33333");
    share_set.erase("33333");
    share_set.erase("22222");

    /*
    pid_t child_pid = fork();    
    if(child_pid)
    {
        share_set.insert("4444444");
        sleep(10);
    }
    */
    {
        share_set.insert("11111");
        share_set.insert("33333");
        share_set.insert("22222");

        sleep(1);
        uint64_t idx = 0;
        Obj *obj = new Obj();
        while(true)
        {
            if(share_set.get_next(idx, *obj))
                printf("%lu %p %s\n", idx, obj, obj->buf);
            else
                break;
        }
    }

    return 0;
}
