#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

#if __GNUC__ == 4 && __GNUC_MINOR__ == 4
#include <string.h>
#else
#include <string>
#endif

#include "net_utility.h"

inline int __getifaddr_ipv4(int ifindex, const char *ifname,
				struct in_addr *ifaddr)
{
	struct ifreq ifreq;
	int sockfd;
	int ret = -1;
	char ifn[IFNAMSIZ];

	if (ifindex > 0)
	{
		if (!(ifname = if_indextoname(ifindex, ifn)))
		{
			errno = ENXIO;
			return -1;
		}
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);
		if ((ret = ioctl(sockfd, SIOCGIFADDR, &ifreq)) >= 0)
			*ifaddr = ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;

		close(sockfd);
	}

	return ret;
}

# define PROC_IFINET6_PATH	"/proc/net/if_inet6"
# define FORMAT_LEN_MAX		53

inline int __getifaddr_ipv6(int ifindex, const char *ifname,
				struct in6_addr *ifaddr)
{
	FILE *fp = fopen(PROC_IFINET6_PATH, "r");
	int ret = -1;

	if (fp)
	{
		char format[FORMAT_LEN_MAX + 1] = "%4s%4s%4s%4s%4s%4s%4s%4s%02x%02x%02x%02x";
		char ifnamesize_format[10];
		sprintf(ifnamesize_format, "%%%ds\n", (int)IFNAMSIZ);
		strcpy(format + strlen(format), ifnamesize_format);

		int index, plen, scope, flags;
		char ifn[IFNAMSIZ];
		char seg[8][5];

		while (fscanf(fp, format, seg[0], seg[1], seg[2], seg[3],
					  seg[4], seg[5], seg[6], seg[7], &index,
					  &plen, &scope, &flags, ifn) != EOF)
		{
			if (ifindex == index || (ifindex == 0 && strcmp(ifn, ifname) == 0))
			{
				char addrstr[INET6_ADDRSTRLEN];
				sprintf(addrstr, "%s:%s:%s:%s:%s:%s:%s:%s", seg[0], seg[1],
						seg[2], seg[3], seg[4], seg[5], seg[6], seg[7]);
				ret = inet_pton(AF_INET6, addrstr, ifaddr);
				goto out;
			}
		}

		errno = ENXIO;
out:
		fclose(fp);
	}

	return ret;
}

int getifaddr(int family, int ifindex, const char *ifname, void *ifaddr)
{
	switch (family)
	{
	case AF_INET:
		return __getifaddr_ipv4(ifindex, ifname, (struct in_addr *)ifaddr);
	case AF_INET6:
		return __getifaddr_ipv6(ifindex, ifname, (struct in6_addr *)ifaddr);
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}
}

int get_local_address(const std::string &eth_name, std::string &local_address)
{
    // get local address
    uint32_t ip; 
    if (getifaddr(AF_INET, 0, eth_name.c_str(), &ip) != 0){
	return -1;
    }
    char ip_str[INET_ADDRSTRLEN]; 
    if(inet_ntop(AF_INET, &ip, ip_str, sizeof (ip_str)) == NULL){
	return -1;
    } 
    local_address = ip_str;
    return 0;
}

struct sockaddr * get_sockaddr_in(const char* ip, uint16_t port)
{
    struct sockaddr_in * addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    memset(addr, 0 , sizeof(struct sockaddr_in));
    //addr->sin_addr = (struct in_addr)inet_addr(ip);
    //if(tmp_addr == INADDR_NONE)
    if(inet_pton(AF_INET, ip, &(addr->sin_addr)) <= 0)
    {
        free(addr);
        return NULL;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = ntohs(port);
    return (sockaddr*)addr;
}

bool get_addr_string(const struct sockaddr* addr, char* addrstr, size_t addrstr_length, uint16_t & port)
{
    strncpy(addrstr, "0.0.0.0", addrstr_length);
    port = 0;
    if (!addr)
        return false;
    const char *ret = NULL;                                           
    switch (addr->sa_family)                                          
    {
    case AF_INET:                                                     
        ret = inet_ntop(addr->sa_family, &((struct sockaddr_in *)addr)->sin_addr, addrstr, addrstr_length);
        if(!ret)
            return false;
        port = htons(((struct sockaddr_in *)addr)->sin_port);
        break;
    case AF_INET6:
        ret = inet_ntop(addr->sa_family, &((struct sockaddr_in6 *)addr)->sin6_addr, addrstr, addrstr_length);
        if(!ret)
            return false;
        port = htons(((struct sockaddr_in6 *)addr)->sin6_port);
        break;
    default:
        break;
    }
    return true;
}

bool get_ai_string(struct addrinfo * ai, char* addrstr, size_t addrstr_length)
{
    size_t cur_len = 0;
    while(ai)
    {
        sockaddr * addr = ai->ai_addr;
        uint16_t port = 0;
        get_addr_string(addr, addrstr + cur_len, addrstr_length - cur_len, port);
        cur_len = strnlen(addrstr, addrstr_length);
        //cur_len += snprintf(addrstr + cur_len, addrstr_length - cur_len,":%hu ", port);
        cur_len += snprintf(addrstr + cur_len, addrstr_length - cur_len," ");
        ai = ai->ai_next;
    }
    return cur_len != 0;
} 

struct addrinfo* copy_addrinfo(struct addrinfo* addr)
{
    if(!addr)
        return NULL;
    unsigned sz = sizeof(struct addrinfo);
    if(addr->ai_family == AF_INET)
        sz += sizeof(struct sockaddr_in);
    else
        sz += sizeof(struct sockaddr_in6);
    struct addrinfo* cur_ai_copy = (struct addrinfo*)malloc(sz);
    memset(cur_ai_copy, 0, sz);
    cur_ai_copy->ai_addr = (struct sockaddr *)(cur_ai_copy + 1); 
    memcpy(cur_ai_copy, addr, sz);
    return cur_ai_copy;
}

struct addrinfo* create_addrinfo(in_addr ip_addr, uint16_t port)
{
    size_t sz = sizeof(struct addrinfo) + sizeof(struct sockaddr_in);
    struct addrinfo*  ai = (struct addrinfo*)malloc(sz);
    memset(ai, 0, sz);
    ai->ai_family  = AF_INET;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_socktype= SOCK_STREAM; 
    ai->ai_addr    = (struct sockaddr *)(ai + 1);
    sockaddr_in * inet_addr =  (struct sockaddr_in*)(ai->ai_addr);
    memcpy(&inet_addr->sin_addr, &ip_addr, sizeof(ip_addr));
    inet_addr->sin_port = port;
    inet_addr->sin_family = AF_INET;
    return ai;
} 

struct addrinfo* create_addrinfo(const char* ip_str, uint16_t port)
{
    size_t sz = sizeof(struct addrinfo) + sizeof(struct sockaddr_in);
    struct addrinfo*  ai = (struct addrinfo*)malloc(sz);
    memset(ai, 0, sz);
    ai->ai_family  = AF_INET;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_socktype= SOCK_STREAM; 
    ai->ai_addr    = (struct sockaddr *)(ai + 1);
    sockaddr_in * inet_addr =  (struct sockaddr_in*)(ai->ai_addr);
    inet_addr->sin_port = ntohs(port);
    inet_addr->sin_family = AF_INET;
    if(inet_pton(AF_INET, ip_str, (void*)&inet_addr->sin_addr) <= 0)
    {
        free(ai);
        return NULL;
    }
    char temp[1024];
    inet_ntop(AF_INET, (void*)&inet_addr->sin_addr, temp, 1024);
    return ai;
}
