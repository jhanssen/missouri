#include "GDICapturer.h"
#include "Encoder.h"
#include "UdpSocket.h"
#include <Windows.h>
#include <Winsock2.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    WSADATA data;
    const int ret = WSAStartup(0x0202, &data);
    if (ret) {
        fprintf(stderr, "WSAStartup failed: %d %s\n", ret, UdpSocket::socketErrorMessage(ret).c_str());
        return 1;
    }

/*
    UdpSocket server;
    if (!server.listen(21047)) {
        WSACleanup();
        return 1;
    }
*/

    GDICapturer cap;
    const uint8_t* buffer = cap.GetBuffer();
    const int32_t width = cap.GetWidth();
    const int32_t height = cap.GetHeight();
    const int32_t size = cap.GetImageSize();
    Encoder enc(buffer, width, height, size);
    const int sleepFor = 1000 / 60;
    for (;;) {
        cap.Capture();
        enc.encode();
        Sleep(sleepFor);
    }

    WSACleanup();
    return 0;
}
