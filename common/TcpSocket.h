#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <cstdint>
#include <string>
#include "Host.h"

class TcpSocketPrivate;

class TcpSocket
{
public:
    TcpSocket();
    ~TcpSocket();

    typedef bool (*CallbackFunc)(const char*, int, void*);
    void setCallback(CallbackFunc callback, void* userData);

    bool connect(const Host& host);
    bool send(const char* data, int size);

    bool isConnected() const;

private:
    // a bit hacky
    void setSocketDescriptor(void* socket);

    TcpSocketPrivate* mPriv;
};

#endif // TCPSOCKET_H
