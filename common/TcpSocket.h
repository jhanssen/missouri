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

    typedef bool (*DataCallbackFunc)(const char*, int, void*);
    void setDataCallback(DataCallbackFunc callback, void* userData);

    typedef void (*CloseCallbackFunc)(void*);
    void setCloseCallback(CloseCallbackFunc, void* userData);

    bool connect(const Host& host);
    bool send(const char* data, int size);

    bool isConnected() const;

    Host remoteHost() const;

private:
    // a bit hacky
    friend class TcpServerPrivate;
    void setSocketDescriptor(void* socket);

    TcpSocketPrivate* mPriv;
};

#endif // TCPSOCKET_H
