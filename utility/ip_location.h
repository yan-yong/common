#ifndef __IP_LOCATION_HPP
#define __IP_LOCATION_HPP
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <map>
#include "singleton/Singleton.h"
#include "log/log.h"

class IpLocation
{
    typedef std::pair<uint32_t, int> addr_val_t; 
    typedef std::map<uint32_t, addr_val_t> addr_map_t;

    addr_map_t addr_map_;

    uint32_t __ip_to_int(const char* ip);

public:
    void initialize(const char* file_name);

    int location(const char* ip);

    DECLARE_SINGLETON(IpLocation);
};

#endif
