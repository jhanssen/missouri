#include "Windows.h"
#include "GDICapturer.h"
#include "Encoder.h"
#include "UdpServer.h"

int main(int argc, char** argv)
{
    UdpServer server(21047);

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
}
