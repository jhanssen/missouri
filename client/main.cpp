#include "UdpSocket.h"
#include "Decoder.h"
#include <unistd.h>
#include <pthread.h>

static bool dataCallback(const char* data, int size, void* userData)
{
    Decoder* decoder = reinterpret_cast<Decoder*>(userData);
    decoder->decode(data, size);
    return true;
}

int main(int argc, char** argv)
{
    Decoder decoder(0, 0);

    UdpSocket socket;
    socket.setCallback(dataCallback, &decoder);
    socket.listen(27584);

    for (;;) {
        usleep(1000000);
    }

    return 0;
}
