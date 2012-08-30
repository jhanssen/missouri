#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <cstdint>
#include <string>

class TcpSocket;
class TcpServerPrivate;

class TcpServer
{
public:
    TcpServer();
    ~TcpServer();

    bool listen(uint16_t port);
    bool isListening() const;

    typedef bool (*CallbackFunc)(TcpSocket* socket, void* userData);
    void setCallback(CallbackFunc callback, void* userData);

private:
    TcpServerPrivate* mPriv;
};

#endif // TCPSERVER_H
