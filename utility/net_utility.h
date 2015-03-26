#ifndef  SSERVICE_LOCALAUX_INC
#define  SSERVICE_LOCALAUX_INC

#include <string>
#include <stdint.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int getifaddr(int family, int ifindex, const char *ifname, void *ifaddr);
int get_local_address(const std::string &eth_name, std::string &local_address);

struct sockaddr * get_sockaddr_in(const char* ip, int port);
bool get_addr_string(const struct sockaddr* addr, char* addrstr, size_t addrstr_length, uint16_t & port);
bool get_ai_string(struct addrinfo * ai, char* addrstr, size_t addrstr_length);
struct addrinfo* copy_addrinfo(struct addrinfo* addr);
struct addrinfo* create_addrinfo(in_addr_t ip_addr, uint16_t port);
struct addrinfo* create_addrinfo(const char* ip_str, uint16_t port);
struct sockaddr* create_sockaddr(const char* ip_addr, uint16_t port);
#endif
