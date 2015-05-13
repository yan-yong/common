#include <arpa/inet.h>
#include "ip_location.h"

DEFINE_SINGLETON(IpLocation);

IpLocation::IpLocation()
{

}

uint32_t IpLocation::__ip_to_int(const char* ip)
{
    in_addr_t addr;
    if(inet_pton(AF_INET, ip, (void *)&addr) <= 0)
    {
        LOG_ERROR("invalid ip %s\n", ip);
        return 0;
    }
    return htonl(addr);
}

void IpLocation::initialize(const char* file_name)
{
    FILE* fid = fopen(file_name, "r");
    if(!fid)
    {
        LOG_ERROR("IpLocation load %s error.\n", file_name);
        exit(1);
    }
    while(true)
    {
        char start_str[20];
        char end_str[20];
        int  type;
        int ret = fscanf(fid, "%s %s %d", start_str, end_str, &type);
        if(ret == EOF)
            break;
        if(ret != 3)
        {
            LOG_ERROR("skip invalid line.\n");
            continue;
        }
        uint32_t start = __ip_to_int(start_str);
        if(!start)
            continue;
        addr_val_t addr_val;
        addr_val.first = __ip_to_int(end_str);
        if(!addr_val.first)
            continue;
        addr_val.second = type;
        addr_map_[start] = addr_val; 
    }
    assert(addr_map_.size() > 0);
}

// > 0: 国内运营商IP
// < 0: 国外运营商IP
int IpLocation::location(const char* ip)
{
    uint32_t ip_val = __ip_to_int(ip);
    addr_map_t::iterator it = addr_map_.upper_bound(ip_val);
    --it;
    addr_val_t addr_val = it->second;
    if(ip_val <= addr_val.first)
        return addr_val.second;
    return -1;
}

