#include "UdpSocket.h"
#include "Decoder.h"
#include "TcpSocket.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

static UdpSocket* stream = 0;

static bool streamCallback(const char* data, int size, void* userData)
{
    Decoder* decoder = reinterpret_cast<Decoder*>(userData);
    decoder->decode(data, size);
    return true;
}

static bool dataCallback(const char* data, int size, void* userData)
{
    Decoder* decoder = reinterpret_cast<Decoder*>(userData);
    decoder->init(0, 0);

    stream = new UdpSocket;
    stream->setCallback(streamCallback, &decoder);
    stream->listen(27584);

    return true;
}

int main(int argc, char** argv)
{
    Decoder decoder;

    TcpSocket socket;
    socket.setCallback(dataCallback, &decoder);
    if (!socket.connect(Host("192.168.11.14", 21047))) {
        fprintf(stderr, "Unable to connect to server\n");
        return 1;
    }

    for (;;) {
        usleep(1000000);
    }

#warning mutex protect this
    delete stream;

    return 0;
}
