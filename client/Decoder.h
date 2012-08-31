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

    void init(const uint8_t* extradata, int extrasize,
              const uint8_t* sps, int spsSize,
              const uint8_t* pps, int ppsSize);
    bool inited() const { return mInited; }

    void decode(const char* data, int size);

private:
    bool mInited;
    uint8_t* mHeader;
    int mHeaderSize;
    struct Buffer {
        char* data;
        uint32_t size;
    };
    std::deque<Buffer> mDatas;
    uint32_t mTotal;
#ifdef OS_Darwin
    VDADecoder mDecoder;
#endif
};

#endif
