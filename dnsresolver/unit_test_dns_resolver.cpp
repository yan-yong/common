#include <assert.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "DNSResolver.hpp"

void fun(DNSResolver::DnsResultType dns_result)
{
    std::string err_msg = dns_result->err_msg_;
    struct addrinfo *ai = dns_result->ai_;
    const void* contex  = dns_result->contex_;
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
    printf("end ...\n");
}

int main()
{
    DNSResolver resolver(5);
    assert(resolver.Open("./resolv.conf") == 0);
    //resolver.Open("");
    std::string host[] = 
    {
        "www.google.com",
        "www.google.com",
        "www.google.com",
        "www.example.com"
    };
    DNSResolver::ResolverCallback callback = fun;
    for(unsigned i = 0; i < sizeof(host)/sizeof(*host); i++)
    {
        resolver.Resolve(host[i], 80, callback, host[i].c_str());
        sleep(1);
    }
    resolver.Close();
    //sleep(1000);
}
