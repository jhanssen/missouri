#ifndef DECODER_H
#define DECODER_H

#include <cstdint>
#ifdef OS_Darwin
# include <VDADecoder.h>
#endif

class Decoder
{
public:
    Decoder(const uint8_t* extradata, int extrasize);
    ~Decoder();

    void decode(const char* data, int size);

private:
#ifdef OS_Darwin
    VDADecoder *mDecoder;
#endif
};

#endif
