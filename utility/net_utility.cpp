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
        if(ret)
            return false;
        port = htons(((struct sockaddr_in *)addr)->sin_port);
        break;
    case AF_INET6:
        ret = inet_ntop(addr->sa_family, &((struct sockaddr_in6 *)addr)->sin6_addr, addrstr, addrstr_length);
        if(ret)
            return false;
        port = htons(((struct sockaddr_in6 *)addr)->sin6_port);
        break;
    default:
        break;
    }
    return true;
}

