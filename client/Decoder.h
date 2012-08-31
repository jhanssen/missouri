#ifndef DECODER_H
#define DECODER_H

#include <cstdint>
#include <deque>
#ifdef OS_Darwin
# include <VDADecoder.h>
#endif

class Decoder
{
public:
    Decoder();
    ~Decoder();

    void init(const uint8_t* extradata, int extrasize);
    bool inited() const { return mInited; }

    void decode(const char* data, int size);

private:
    bool mInited;
    struct Buffer {
        char* data;
        int size;
    };
    std::deque<Buffer> mDatas;
    int mTotal;
#ifdef OS_Darwin
    VDADecoder mDecoder;
#endif
};

#endif
