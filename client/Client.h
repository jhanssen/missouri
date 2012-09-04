#ifndef CLIENT_H
#define CLIENT_H

#include "TcpSocket.h"
#include "UdpSocket.h"
#include "Receiver.h"
#include "Decoder.h"
#include <string>

class Client
{
public:
    Client(int width, int height, const std::string& hostname);

private:
    static bool streamCallback(const char* data, int size, void* userData);
    static bool controlCallback(const char* data, int size, void* userData);

private:
    TcpSocket control;
    UdpSocket stream;
    Receiver receiver;
    Decoder decoder;

    int outputWidth, outputHeight;

    char* sps;
    int spss;
    char* pps;
    int ppss;
};

#endif
