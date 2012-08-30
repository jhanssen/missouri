#include "GDICapturer.h"
#include "Encoder.h"
#include "TcpServer.h"
#include "TcpSocket.h"
#include "UdpSocket.h"
#include <Windows.h>
#include <Winsock2.h>
#include <stdio.h>
#include <assert.h>

static bool newSocket(TcpSocket* socket, void* userData)
{
    Encoder* enc = static_cast<Encoder*>(userData);
    printf("got connection, encoder %p\n", enc);

    uint8_t* payload;
    int32_t size;

    int total = enc->headerSize();
    assert(total > 0);
    size = 4;
    socket->send(reinterpret_cast<char*>(&size), 4);
    socket->send(reinterpret_cast<char*>(&total), 4);

    enc->getSps(&payload, &size);
    socket->send(reinterpret_cast<char*>(&size), 4);
    socket->send(reinterpret_cast<char*>(payload), size);

    enc->getPps(&payload, &size);
    socket->send(reinterpret_cast<char*>(&size), 4);
    socket->send(reinterpret_cast<char*>(payload), size);
}

int main(int argc, char** argv)
{
    WSADATA data;
    const int ret = WSAStartup(0x0202, &data);
    if (ret) {
        fprintf(stderr, "WSAStartup failed: %d %s\n", ret, UdpSocket::socketErrorMessage(ret).c_str());
        return 1;
    }

    GDICapturer cap;
    const uint8_t* buffer = cap.GetBuffer();
    const int32_t width = cap.GetWidth();
    const int32_t height = cap.GetHeight();
    const int32_t size = cap.GetImageSize();
    Encoder enc(buffer, width, height, size);

    const uint16_t port = 21047;
    TcpServer server;
    server.setCallback(newSocket, &enc);
    if (!server.listen(port)) {
        fprintf(stderr, "Server listen failed\n");
        WSACleanup();
        return 1;
    }
    fprintf(stdout, "Server listening on port %u\n", port);

    const int sleepFor = 1000 / 60;
    for (;;) {
        cap.Capture();
        enc.encode();
        Sleep(sleepFor);
    }

    WSACleanup();
    return 0;
}
