#include "DNSResolver.hpp"

struct addrinfo* DNSResolver::__copy_addrinfo(struct evutil_addrinfo *addr)
{
    unsigned addr_sz = 0;
    if(addr->ai_family == AF_INET6)
        addr_sz = sizeof(struct sockaddr_in6);
    else
        addr_sz = sizeof(struct sockaddr_in);
    unsigned sz = sizeof(struct addrinfo) + addr_sz;
    struct addrinfo* ai = (struct addrinfo*)malloc(sz);
    memset(ai, 0, sz);

    ai->ai_addr = (sockaddr*)(ai + 1);
    memcpy(ai->ai_addr, addr->ai_addr, addr_sz);
    ai->ai_flags = addr->ai_flags;
    ai->ai_family= addr->ai_family;
    ai->ai_socktype = addr->ai_socktype;
    ai->ai_protocol = addr->ai_protocol;
    ai->ai_addrlen  = addr_sz;
    if(addr->ai_canonname)
        ai->ai_canonname= strdup(addr->ai_canonname);
    return ai;
}

void DNSResolver::__internal_callback(int errcode, struct evutil_addrinfo *addr, void *contex)
{
    struct RequestItem* request = (struct RequestItem*)contex;
    const void* user_contex = request->contex_;
    if(errcode)
    {
        DnsResultType dns_result(new ResultItem(
            evutil_gai_strerror(errcode), NULL, request->contex_));
        request->callback_(dns_result); 
        delete request;
        return;
    }

    struct addrinfo *dns_ret = NULL;
    struct addrinfo *last_ai = NULL;
    for (struct evutil_addrinfo *ai = addr; ai; ai = ai->ai_next) 
    {
        if(!dns_ret)
        {
            dns_ret = __copy_addrinfo(ai);
            last_ai = dns_ret;
        }
        else
        {
            last_ai->ai_next = __copy_addrinfo(ai);
            last_ai = last_ai->ai_next;
        }
    }
    evutil_freeaddrinfo(addr);
    DnsResultType dns_result(new ResultItem("", dns_ret, user_contex));
    request->callback_(dns_result); 
    delete request;
}

int DNSResolver::Open(std::string filename)
{
    base_ = event_base_new();
    if(!base_)
        return -1;
    if(filename.empty())
        dnsbase_ = evdns_base_new(base_, 1);
    else
    {
        dnsbase_ = evdns_base_new(base_, 0);
        if(evdns_base_resolv_conf_parse(dnsbase_, DNS_OPTIONS_ALL, filename.c_str()) != 0)
            return -1;
    }
    pthread_create(&pid_, NULL, DNSResolver::runtine, this);
    return 0;
}

void DNSResolver::Resolve(const std::string& host, uint16_t port,
        ResolverCallback cb, const void* contex)
{
    printf("put %s\n", host.c_str());
    char port_str[10];
    snprintf(port_str, 10, "%hu", port);
    RequestItem* request = new RequestItem(cb, contex);
    //struct evdns_getaddrinfo_request *req = 
    evdns_getaddrinfo(dnsbase_, host.c_str(), port_str, &hints_, 
            DNSResolver::__internal_callback, request);
}
