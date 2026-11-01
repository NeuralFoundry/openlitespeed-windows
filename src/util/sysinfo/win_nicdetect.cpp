#include <iphlpapi.h>

#ifndef IFF_UP
#define IFF_UP          0x1
#define IFF_BROADCAST   0x2
#define IFF_LOOPBACK    0x8
#define IFF_MULTICAST   0x1000
#endif

struct ifi_info *NICDetect::get_ifi_info(int family, int doaliases)
{
    struct ifi_info *head = NULL, **tail = &head;
    IP_ADAPTER_ADDRESSES *addrs = NULL, *a;
    ULONG len = 16384, ret;
    int tries;

    for (tries = 0; tries < 4; ++tries)
    {
        addrs = (IP_ADAPTER_ADDRESSES *)malloc(len);
        if (!addrs)
            return NULL;
        ret = GetAdaptersAddresses(family == AF_INET6 ? AF_INET6 : AF_INET,
                                   GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                                   | GAA_FLAG_SKIP_DNS_SERVER, NULL, addrs, &len);
        if (ret == ERROR_SUCCESS)
            break;
        free(addrs);
        addrs = NULL;
        if (ret != ERROR_BUFFER_OVERFLOW)
            return NULL;
    }
    if (!addrs)
        return NULL;

    for (a = addrs; a != NULL; a = a->Next)
    {
        IP_ADAPTER_UNICAST_ADDRESS *u;
        int first = 1;
        if (a->OperStatus != IfOperStatusUp)
            continue;
        for (u = a->FirstUnicastAddress; u != NULL; u = u->Next)
        {
            struct ifi_info *ifi;
            int slen = u->Address.iSockaddrLength;
            if (u->Address.lpSockaddr->sa_family != family)
                continue;
            if (!first && !doaliases)
                break;
            ifi = (struct ifi_info *)calloc(1, sizeof(struct ifi_info));
            if (!ifi)
                break;
            WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, ifi->ifi_name,
                                IFI_NAME - 1, NULL, NULL);
            ifi->ifi_hlen = (u_short)(a->PhysicalAddressLength > IFI_HADDR
                                      ? IFI_HADDR : a->PhysicalAddressLength);
            memcpy(ifi->ifi_haddr, a->PhysicalAddress, ifi->ifi_hlen);
            ifi->ifi_flags = IFF_UP;
            if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                ifi->ifi_flags |= IFF_LOOPBACK;
            else
                ifi->ifi_flags |= IFF_BROADCAST | IFF_MULTICAST;
            ifi->ifi_myflags = first ? 0 : IFI_ALIAS;
            ifi->ifi_addr = (struct sockaddr *)calloc(1, slen);
            if (ifi->ifi_addr)
                memcpy(ifi->ifi_addr, u->Address.lpSockaddr, slen);
            *tail = ifi;
            tail = &ifi->ifi_next;
            first = 0;
        }
    }
    free(addrs);
    return head;
}
