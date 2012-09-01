#ifndef CLIENT_H
#define CLIENT_H

#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Receiver.h"
#include "Decoder.h"

class Client
{
public:
    Client();

private:
    static bool streamCallback(const char* data, int size, void* userData);
    static bool controlCallback(const char* data, int size, void* userData);

private:
    TcpSocket control;
    UdpSocket stream;
    Receiver receiver;
    Decoder decoder;

    char* sps;
    int spss;
    char* pps;
    int ppss;
};

#endif