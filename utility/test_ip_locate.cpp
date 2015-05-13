#include "ip_location.h"

int main()
{
    IpLocation::Instance()->initialize("internal_ip.dat");
    printf("%d\n", IpLocation::Instance()->location("1.2.6.5"));
    printf("%d\n", IpLocation::Instance()->location("14.224.255.255"));
    printf("%d\n", IpLocation::Instance()->location("14.223.255.254"));
    printf("%d\n", IpLocation::Instance()->location("223.210.255.255"));
}
