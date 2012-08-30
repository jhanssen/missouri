#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <cstdint>
#include "Host.h"

class UdpSocketPrivate;

class UdpSocket
{
public:
    UdpSocket();
    ~UdpSocket();

    bool listen(uint16_t port);
    bool isListening() const;

    bool send(const Host& host, const char* data, int size);

private:
    UdpSocketPrivate* mPriv;
};

#endif // UDPSOCKET_H
