#include <winsock2.h>
#include <ws2tcpip.h>
#include "udns.h"

const char *dns_ntop(int af, const void *src, char *dst, int size)
{
    return inet_ntop(af, src, dst, (size_t)size);
}

int dns_pton(int af, const char *src, void *dst)
{
    return inet_pton(af, src, dst);
}
