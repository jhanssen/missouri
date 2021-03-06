#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <cstdint>
#include <string>
#include "Host.h"

class UdpSocketPrivate;

class UdpSocket
{
public:
    UdpSocket();
    ~UdpSocket();

    bool listen(uint16_t port);
    bool isListening() const;

    typedef bool (*CallbackFunc)(const char*, int, void*);
    void setCallback(CallbackFunc callback, void* userData);

    bool send(const Host& host, const char* data, int size);

    static std::string socketErrorMessage(int error);

private:
    UdpSocketPrivate* mPriv;
};

#endif // UDPSOCKET_H
