#ifndef UDPSOCKET_H
#define UDPSOCKET_H

class UdpSocketPrivate;

class UdpSocket
{
public:
    UdpSocket(int port);
    ~UdpSocket();

    bool isListening() const;

private:
    UdpSocketPrivate* mPriv;
};

#endif // UDPSOCKET_H
