#ifndef OLS_IFADDRS_H
#define OLS_IFADDRS_H

#include "ols_win.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ifaddrs
{
    struct ifaddrs *ifa_next;
    char *ifa_name;
    unsigned int ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    struct sockaddr *ifa_broadaddr;
    void *ifa_data;
};
#define ifa_dstaddr ifa_broadaddr

int getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifa);

#ifdef __cplusplus
}
#endif

#endif
