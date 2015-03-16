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


class DNSResolver
{
public:
    typedef boost::function<void (std::string, struct addrinfo*, const void*)> ResolverCallback;

protected:
    struct RequestItem
    {
        ResolverCallback& callback_;
        const void* contex_;
        RequestItem(ResolverCallback& callback, const void* contex):
            callback_(callback), contex_(contex)
        {}
    };

    struct event_base *base_;
    struct evdns_base *dnsbase_;
    struct evutil_addrinfo hints_;
    pthread_t pid_;

    static struct addrinfo* __copy_addrinfo(struct evutil_addrinfo *addr); 
    static void __internal_callback(int errcode, struct evutil_addrinfo *addr, void *contex);

    static void* runtine(void* arg)
    {
        DNSResolver* resolver = (DNSResolver*)arg;
        assert(resolver->base_);
        event_base_dispatch(resolver->base_);
        return NULL;
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

    void Close()
    {
        event_base_loopexit(base_, NULL);
        evdns_base_free(dnsbase_, 0);
        dnsbase_ = NULL;
        event_base_free(base_);
        base_ = NULL; 
        pthread_join(pid_, NULL);
    }

    int Open(std::string filename = "");
  
    void Resolve(const std::string& host, uint16_t port,
        ResolverCallback& cb, const void* contex);
};

#endif
