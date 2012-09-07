#include "Client.h"
#include <string>
#include <stdio.h>
#include <assert.h>

#define UDP_PORT 21048

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

Client::Client(int width, int height, const std::string& hostname,
               HeaderCallbackFunc callback, void* userData)
    : outputWidth(0), outputHeight(0), sps(0), spss(0), pps(0), ppss(0),
      headerCallback(callback), headerUserData(userData)
{
    stream.setCallback(streamCallback, this);
    control.setDataCallback(controlCallback, this);
    if (!control.connect(Host(hostname, 21047))) {
        fprintf(stderr, "Unable to connect to server\n");
        return;
    }

    char data[14];
    int sz = htonl(10);
    uint32_t w = htonl(width), h = htonl(height);
    uint16_t p = htons(UDP_PORT);
    memcpy(data, &sz, 4);
    memcpy(data + 4, &p, 2);
    memcpy(data + 6, &w, 4);
    memcpy(data + 10, &h, 4);
    control.send(data, 14);
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
    //printf("feeding %d bytes\n", size);
    client->receiver.feed(data, size);
    if (!client->outputWidth) {
        char* buf;
        int size;
        if (!client->receiver.popBlock(&buf, &size))
            return true;
        if (size != 8)
            return true;
        client->outputWidth = ntohl(*reinterpret_cast<int*>(buf));
        client->outputHeight = ntohl(*reinterpret_cast<int*>(buf + 4));

        if (client->headerCallback)
            client->headerCallback(client->outputWidth, client->outputHeight, client->headerUserData);
    }
    if (!client->sps) {
        //printf("testing sps\n");
        if (!client->receiver.popBlock(&client->sps, &client->spss))
            return true;
        //printf("got sps of size %d\n", client->spss);
    }
    if (!client->pps) {
        //printf("testing pps\n");
        if (!client->receiver.popBlock(&client->pps, &client->ppss))
            return true;
        //printf("got pps of size %d\n", client->ppss);
    }
    assert(client->pps && client->sps);
    assert(client->ppss > 0 && client->spss > 0);
    if (!client->decoder.inited()) {
        std::string extra = makeExtraData(reinterpret_cast<uint8_t*>(client->sps), client->spss,
                                          reinterpret_cast<uint8_t*>(client->pps), client->ppss);
        client->decoder.init(client->outputWidth, client->outputHeight, extra);

        client->stream.listen(UDP_PORT);
    }
    free(client->pps);
    free(client->sps);

    return true;
}
