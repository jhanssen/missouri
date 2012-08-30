#include "Host.h"
#ifdef OS_Windows
# include <Winsock2.h>
// Silly Windows, the following is required to get getaddrinfo/freeaddrinfo apparently
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0501
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
