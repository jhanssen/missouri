#include "Client.h"
#include <string>
#include <stdio.h>

static inline std::string makeExtraData(unsigned char* sps, int spss, unsigned char* pps, int ppss)
{
    const int total = (spss + 4 + ppss + 4) * 4 + 100;
    std::string extra(total, '\0');

    // skip NAL unit type
    extra[0] = 0x1;
    extra[1] = sps[1];
    extra[2] = sps[2];
    extra[3] = sps[3];
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
    printf("feeding %d bytes\n", size);
    client->receiver.feed(data, size);
    if (!client->sps) {
        printf("testing sps\n");
        if (!client->receiver.popBlock(&client->sps, &client->spss))
            return true;
        printf("got sps of size %d\n", client->spss);
    }
    if (!client->pps) {
        printf("testing pps\n");
        if (!client->receiver.popBlock(&client->pps, &client->ppss))
            return true;
        printf("got pps of size %d\n", client->ppss);
    }
    assert(client->pps && client->sps);
    assert(client->ppss > 0 && client->spss > 0);
    if (!client->decoder.inited()) {
        std::string extra = makeExtraData(reinterpret_cast<uint8_t*>(client->sps), client->spss,
                                          reinterpret_cast<uint8_t*>(client->pps), client->ppss);
        client->decoder.init(reinterpret_cast<const uint8_t*>(extra.data()), extra.size());

        client->stream.listen(27584);
    }
    free(client->pps);
    free(client->sps);

    return true;
}
