#include "UdpSocket.h"
#include "Decoder.h"
#include "TcpSocket.h"
#include "Receiver.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

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

static inline std::string makeExtraData(char* sps, int spss, char* pps, int ppss)
{
    const int total = (spss + 4 + ppss + 4) * 4 + 100;
    std::string extra(total, '\0');

    // skip NAL unit type
    ++sps;
    extra[0] = 0x1;
    extra[1] = sps[0];
    extra[2] = sps[1];
    extra[3] = sps[2];
    extra[4] = 0xfc | (4 - 1);

    int sz = 5;
    uint16_t num;

    extra[sz++] = 0xe0 | 1;

    memcpy(&extra[sz + 2], sps, spss);
    num = htons(spss);
    memcpy(&extra[sz], &num, sizeof(uint16_t));
    sz += spss + 2;

    extra[sz++] = 1;

    memcpy(&extra[sz + 2], pps, ppss);
    num = htons(ppss);
    memcpy(&extra[sz], &num, sizeof(uint16_t));
    sz += ppss + 2;

    extra.resize(sz);
    return extra;
}

Client::Client()
    : sps(0), spss(0), pps(0), ppss(0)
{
    stream.setCallback(streamCallback, this);
    control.setCallback(controlCallback, this);
    if (!control.connect(Host("192.168.11.14", 21047))) {
        fprintf(stderr, "Unable to connect to server\n");
    }
}

bool Client::streamCallback(const char* data, int size, void* userData)
{
    Client* client = reinterpret_cast<Client*>(userData);
    client->decoder.decode(data, size);
    return true;
}

bool Client::controlCallback(const char* data, int size, void* userData)
{
    Client* client = reinterpret_cast<Client*>(userData);
    client->receiver.feed(data, size);
    if (!client->sps) {
        if (!client->receiver.popBlock(&client->sps, &client->spss))
            return true;
    }
    if (!client->pps) {
        if (!client->receiver.popBlock(&client->pps, &client->ppss))
            return true;
    }
    assert(client->pps && client->sps);
    assert(client->ppss > 0 && client->spss > 0);
    if (!client->decoder.inited()) {
        std::string extra = makeExtraData(client->sps, client->spss, client->pps, client->ppss);
        client->decoder.init(reinterpret_cast<const uint8_t*>(extra.data()), extra.size());

        client->stream.listen(27584);
    }
    free(client->pps);
    free(client->sps);

    return true;
}

int main(int argc, char** argv)
{
    Client client;

    for (;;) {
        usleep(1000000);
    }

    return 0;
}
