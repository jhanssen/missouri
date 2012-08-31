#ifndef DECODER_H
#define DECODER_H

#include <cstdint>

class DecoderPrivate;

class Decoder
{
public:
    Decoder();
    ~Decoder();

    void init(const uint8_t* extradata, int extrasize,
              const uint8_t* sps, int spsSize,
              const uint8_t* pps, int ppsSize);
    bool inited() const;

    void decode(const char* data, int size);

private:
    DecoderPrivate* mPriv;
};

#endif
