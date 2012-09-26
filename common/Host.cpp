#include "Host.h"
#ifdef OS_Windows
# include <Winsock2.h>
# include <Ws2TcpIp.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>

Host::Host()
    : mData(0)
{
}

Host::Host(const std::string& address, uint16_t port)
    : mData(0)
{
    addrinfo hints;
    addrinfo* result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // Only allow IPv4

    const int ret = getaddrinfo(address.c_str(), 0, &hints, &result);
    if (ret) {
        fprintf(stderr, "getaddrinfo failed: %d %s\n", ret, gai_strerror(ret));
        return;
    }

    assert(result);
    assert(result->ai_family == AF_INET);

    // pick the first returned result
    const sockaddr_in* sa = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    mData = sa->sin_addr.s_addr;

    freeaddrinfo(result);

    mData |= (static_cast<uint64_t>(port) << 48);
}

Host::Host(uint32_t address, uint16_t port)
{
    mData = address;
    mData |= (static_cast<uint64_t>(port) << 48);
}
