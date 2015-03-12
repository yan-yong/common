#include <assert.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "DNSResolver.hpp"

void fun(std::string err_msg, struct addrinfo *ai, const void* contex)
{
    if(ai)
    {
        while(ai)
        {
            char buf[128];
            const char *s = NULL;
            if (ai->ai_family == AF_INET) 
            {
                struct sockaddr_in *sin = (struct sockaddr_in *)ai->ai_addr;
                s = inet_ntop(AF_INET, &sin->sin_addr, buf, 128);
            } 
            else if (ai->ai_family == AF_INET6) 
            {
                struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ai->ai_addr;
                s = inet_ntop(AF_INET6, &sin6->sin6_addr, buf, 128);
            }
            printf("%s --> %s\n", (char*)contex, buf);
            ai = ai->ai_next;
        }
    }
    else
    {
        printf("%s FAILED: %s\n", (char*)contex, err_msg.c_str());
    }
}

int main()
{
    DNSResolver resolver;
    assert(resolver.Open("./resolv.conf") == 0);
    std::string host[] = 
    {
        "www.bing1.com.cn"
        /*
        ,
        "www.baidu.com",
        "www.sina.com",
        "www.google.com",
        "www.google.com.hk",
        "www.google.com.tw",
        "www.renren.com",
        "www.bing.com",
        "www.ask.com",
        "www.china.com",
        "www.bing.com.cn",
        "www.baidu.com",
        "www.sina.com",
        "www.google.com",
        "www.google.com.hk",
        "www.google.com.tw",
        "www.renren.com",
        "www.bing.com",
        "www.ask.com",
        "www.china.com"
        */
    };
    ResolverCallback callback = fun;
    for(unsigned i = 0; i < sizeof(host)/sizeof(*host); i++)
    {
        resolver.Resolve(host[i], 80, callback, host[i].c_str());
    }
    sleep(1000);
}
