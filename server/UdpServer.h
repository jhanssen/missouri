#ifndef UDPSERVER_H
#define UDPSERVER_H

class UdpServerPrivate;

class UdpServer
{
public:
    UdpServer(int port);
    ~UdpServer();

    bool isListening() const;

private:
    UdpServerPrivate* mPriv;
};

#endif // UDPSERVER_H
