#include "DenseBitmap.h"
#include <iostream>
#include <assert.h>
#include "log/log.h"

using namespace std;

int main()
{
    DenseBitmap bitmap;
    if(bitmap.initialize(32, "a.dat") < 0)
        assert(false);
    bitmap.set_save_interval(1000, 2000);
    for(int i = 0; i < 500000; i++){
        char buf[1024];
        snprintf(buf, 1024, "%d", i);
        bitmap.set(buf);
        //assert(bitmap.get(i));
        //bitmap.set(rand());
        //assert(bitmap.get(rand()));
        //assert(bitmap.get(buf) != 0);
        if(i % 10000 == 0)
            printf("Processing %d\n", i);
    }
    //assert(bitmap.get(0));
    //bitmap.set(0);
    //assert(bitmap.get(0) == 1);
    sleep(1);
    bitmap.exit();
    //std::cout << bitmap.get("http://www.baidu.com") << std::endl;
    //std::cout << bitmap.get("http://www.sina.com") << std::endl;
}
