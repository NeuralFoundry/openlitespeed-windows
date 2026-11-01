#include "compat_internal.h"
#include <iphlpapi.h>
#include <ifaddrs.h>

int getifaddrs(struct ifaddrs **ifap)
{
    IP_ADAPTER_ADDRESSES *addrs = NULL, *a;
    struct ifaddrs *head = NULL, **tail = &head;
    ULONG len = 16384, ret = ERROR_BUFFER_OVERFLOW;
    int tries;
    for (tries = 0; tries < 4 && ret == ERROR_BUFFER_OVERFLOW; ++tries)
    {
        free(addrs);
        addrs = (IP_ADAPTER_ADDRESSES *)malloc(len);
        if (!addrs)
        {
            errno = ENOMEM;
            return -1;
        }
        ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                                   | GAA_FLAG_SKIP_DNS_SERVER, NULL, addrs, &len);
    }
    if (ret != ERROR_SUCCESS)
    {
        free(addrs);
        errno = EIO;
        return -1;
    }
    for (a = addrs; a; a = a->Next)
    {
        IP_ADAPTER_UNICAST_ADDRESS *u;
        for (u = a->FirstUnicastAddress; u; u = u->Next)
        {
            struct ifaddrs *ifa = (struct ifaddrs *)calloc(1, sizeof(*ifa));
            int slen = u->Address.iSockaddrLength;
            int nlen;
            if (!ifa)
                break;
            nlen = WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, NULL, 0, NULL, NULL);
            ifa->ifa_name = (char *)malloc(nlen > 0 ? nlen : 1);
            if (ifa->ifa_name)
            {
                ifa->ifa_name[0] = 0;
                WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, ifa->ifa_name, nlen, NULL, NULL);
            }
            ifa->ifa_flags = (a->OperStatus == IfOperStatusUp ? 0x1 | 0x40 : 0)
                             | (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK ? 0x8 : 0x2);
            ifa->ifa_addr = (struct sockaddr *)calloc(1, slen);
            if (ifa->ifa_addr)
                memcpy(ifa->ifa_addr, u->Address.lpSockaddr, slen);
            *tail = ifa;
            tail = &ifa->ifa_next;
        }
    }
    free(addrs);
    *ifap = head;
    return 0;
}

void freeifaddrs(struct ifaddrs *ifa)
{
    while (ifa)
    {
        struct ifaddrs *n = ifa->ifa_next;
        free(ifa->ifa_name);
        free(ifa->ifa_addr);
        free(ifa->ifa_netmask);
        free(ifa->ifa_broadaddr);
        free(ifa);
        ifa = n;
    }
}
