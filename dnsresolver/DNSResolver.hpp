#ifndef __DNS_RESOLVER_HPP
#define __DNS_RESOLVER_HPP

#include <event2/dns.h>
#include <event2/util.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <boost/bind.hpp>
#include <boost/function.hpp>

typedef boost::function<void (std::string, struct addrinfo*, const void*)> ResolverCallback;

class DNSResolver
{
    struct RequestItem
    {
        ResolverCallback& callback_;
        const void* contex_;
        RequestItem(ResolverCallback& callback, const void* contex):
            callback_(callback), contex_(contex)
        {}
    };

protected:
    struct event_base *base_;
    struct evdns_base *dnsbase_;
    struct evutil_addrinfo hints_;
    pthread_t pid_;

    static struct addrinfo* __copy_addrinfo(struct evutil_addrinfo *addr)
    {
        unsigned addr_sz = 0;
        if(addr->ai_family == AF_INET6)
            addr_sz = sizeof(struct sockaddr_in6);
        else
            addr_sz = sizeof(struct sockaddr_in);
        unsigned sz = sizeof(struct addrinfo) + addr_sz;
        struct addrinfo* ai = (struct addrinfo*)malloc(sz);
        memset(ai, 0, sz);

        ai->ai_addr = (sockaddr*)ai + 1;
        memcpy(ai, addr, sz);
        ai->ai_flags = addr->ai_flags;
        ai->ai_family= addr->ai_family;
        ai->ai_socktype = addr->ai_socktype;
        ai->ai_protocol = addr->ai_protocol;
        ai->ai_addrlen  = addr_sz;
        if(addr->ai_canonname)
            ai->ai_canonname= strdup(addr->ai_canonname);
        return ai;
    } 

    static void __internal_callback(int errcode, struct evutil_addrinfo *addr, void *contex)
    {
        struct RequestItem* request = (struct RequestItem*)contex;
        const void* user_contex = request->contex_;
        ResolverCallback& cb = request->callback_;
        if(errcode)
        {
            cb(evutil_gai_strerror(errcode), NULL, request->contex_); 
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
        delete request;
        evutil_freeaddrinfo(addr);
        cb("", dns_ret, user_contex); 
    }

    static void* runtine(void* arg)
    {
        DNSResolver* resolver = (DNSResolver*)arg;
        assert(resolver->base_);
        event_base_dispatch(resolver->base_);
    }

public:
    DNSResolver(): base_(NULL), dnsbase_(NULL)
    {
        memset(&hints_, 0, sizeof(hints_));
        //hints_.ai_family = AF_UNSPEC;
        hints_.ai_family = AF_INET;
        hints_.ai_flags = EVUTIL_AI_CANONNAME;
        hints_.ai_socktype = SOCK_STREAM;
        hints_.ai_protocol = IPPROTO_TCP;
    }

    void SetUdpProtocal()
    {
        hints_.ai_socktype = SOCK_DGRAM;
        hints_.ai_protocol = IPPROTO_UDP;
    }

    int Open(std::string filename = "")
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

    void Close()
    {
        event_base_loopexit(base_, NULL);
        evdns_base_free(dnsbase_, 0);
        dnsbase_ = NULL;
        event_base_free(base_);
        base_ = NULL; 
        pthread_join(pid_, NULL);
    }

    void Resolve(const std::string& host, uint16_t port,
        ResolverCallback& cb, const void* contex)
    {
        printf("put %s\n", host.c_str());
        char port_str[10];
        snprintf(port_str, 10, "%hu", port);
        RequestItem* request = new RequestItem(cb, contex);
        struct evdns_getaddrinfo_request *req = evdns_getaddrinfo(dnsbase_, host.c_str(), port_str, &hints_, 
            DNSResolver::__internal_callback, request);
    }
};

#endif
