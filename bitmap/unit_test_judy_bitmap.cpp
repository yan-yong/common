#include "JudyBitmap.h"

int main(int argc, char* argv[])
{
    if(argc != 2){
        fprintf(stderr, "need one argument\n"); 
        return 1;
    }

    JudyBitmap bitmap;
    bitmap.initialize(0, "aa.txt");

    std::string url1 = "http://www.baidu.com/";
    std::string url2 = "http://www.sina.com/";

    int ret = bitmap.get(url1);
    printf("get %s\t%d\n", url1.c_str(), ret);

    ret = bitmap.set(url1);
    printf("set %s\t%d\n", url1.c_str(), ret);
    ret = bitmap.set(url1);
    printf("set %s\t%d\n", url1.c_str(), ret);
    ret = bitmap.set(url1);
    printf("set %s\t%d\n", url1.c_str(), ret);

    ret = bitmap.unset(url2);
    printf("unset %s\t%d\n", url2.c_str(), ret);
    ret = bitmap.unset(url1);
    printf("unset %s\t%d\n", url1.c_str(), ret);
    ret = bitmap.unset(url1);
    printf("unset %s\t%d\n", url1.c_str(), ret);
    ret = bitmap.unset(url1);
    printf("unset %s\t%d\n", url1.c_str(), ret);

    ret = bitmap.get(url1);
    printf("get %s\t%d\n", url1.c_str(), ret);

    ret = bitmap.set(url1);
    printf("set %s\t%d\n", url1.c_str(), ret);

    for(int i = 0; i < 100000; i++){
        int num = rand() % 50;
        char buf[1025];
        for(int j = 0; j < num; j++) 
            snprintf(buf+j, 1024, "%c", 'a'+ rand()%26);
        buf[num] = 0;
        std::string url = buf;
        //fprintf(stderr, "insert %s\n", buf);
        bitmap.set(url); 
    }
    /*
    */

    return 0;
}
