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
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>
#include "utility/murmur_hash.h"
#include "utility/net_utility.h"

class DNSResolver
{
public:
    struct ResultItem
    {
        std::string      err_msg_;
        struct addrinfo* ai_;
        const void*      contex_;
        time_t           update_time_;
        ResultItem(std::string err_msg, struct addrinfo* ai, 
            const void* contex):
            err_msg_(err_msg), ai_(ai), 
            contex_(contex), update_time_(time(NULL))
        {}
        ~ResultItem()
        {
            freeaddrinfo(ai_);
        }
        bool GetAddr(std::string& addr, uint16_t& port)
        {
            size_t num = 0;
            struct addrinfo* ai = ai_;
            while(ai)
            {
                ++num;
                ai = ai->ai_next; 
            }
            int rdx = rand() % num;
            int i = 0;
            for(i = 0, ai = ai_; i < rdx; ++i)
                ai = ai->ai_next;
            struct sockaddr* sock = ai->ai_addr;
            char buf[100];
            if(!get_addr_string(sock, buf, 100, port))
                return false;
            addr = buf;
            return true;
        }
    };
    typedef boost::shared_ptr<ResultItem> DnsResultType;
    typedef boost::function<void (DnsResultType)> ResolverCallback;
    typedef boost::unordered_map<uint64_t, DnsResultType> DnsCache;
    typedef struct evdns_getaddrinfo_request* DnsReqKey;

protected:
    struct RequestItem
    {
        ResolverCallback callback_;
        const void* contex_;
        std::string host_;
        uint16_t    port_;
        RequestItem(const std::string& host, uint16_t port,  
            ResolverCallback callback, const void* contex):
            callback_(callback), contex_(contex), 
            host_(host), port_(port)
        {}
        uint64_t cache_key() const
        {
            std::string key = host_ + ":" + boost::lexical_cast<std::string>(port_);
            uint64_t ukey;
            MurmurHash_x64_64(key.c_str(), key.size(), &ukey);
            return ukey;
        }
    };

    struct event_base *base_;
    struct evdns_base *dnsbase_;
    struct evutil_addrinfo hints_;
    pthread_t pid_;
    static time_t dns_cache_time_;
    static DnsCache*   dns_cache_;
    bool   closed_;

    static struct addrinfo* __copy_addrinfo(struct evutil_addrinfo *addr); 
    static void __internal_callback(int errcode, struct evutil_addrinfo *addr, void *contex);
    static DnsResultType __get_dns_cache(struct RequestItem*);
    static void __set_dns_cache(struct RequestItem*, DnsResultType);

    static void* runtine(void* arg)
    {
        DNSResolver* resolver = (DNSResolver*)arg;
        assert(resolver->base_);
        event_base_dispatch(resolver->base_);
        return NULL;
    }

public:
    DNSResolver(time_t dns_cache_time = 0): 
        base_(NULL), dnsbase_(NULL), 
        closed_(false)
    {
        if(!dns_cache_)
            dns_cache_ = new DnsCache();
        memset(&hints_, 0, sizeof(hints_));
        //hints_.ai_family = AF_UNSPEC;
        hints_.ai_family = AF_INET;
        hints_.ai_flags = EVUTIL_AI_CANONNAME;
        hints_.ai_socktype = SOCK_STREAM;
        hints_.ai_protocol = IPPROTO_TCP;
        dns_cache_time_ = dns_cache_time;
    }

    ~DNSResolver()
    {
        Close();
    }

    void SetUdpProtocal()
    {
        hints_.ai_socktype = SOCK_DGRAM;
        hints_.ai_protocol = IPPROTO_UDP;
    }

    void Close();

    int Open(std::string filename = "");
  
    DNSResolver::DnsReqKey Resolve(const std::string& host, 
        uint16_t port, ResolverCallback cb, const void* contex);

    void Cancel(DNSResolver::DnsReqKey req_key);
};

#endif
