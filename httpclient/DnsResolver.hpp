#ifndef __DNS_RESOLVER_H
#define __DNS_RESOLVER_H

class DnsResolver
{
public:
    virtual struct addrinfo* Resolve(const std::string& host, const std::string& port)
    {
    
    } 
};

#endif

