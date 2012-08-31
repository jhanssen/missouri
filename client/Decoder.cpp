#include "Decoder.h"
#include <stdio.h>
#include <assert.h>
#include <deque>
#ifdef OS_Darwin
# include <VDADecoder.h>
#endif
extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

class DecoderPrivate
{
public:
    bool inited;
    uint8_t* header;
    int headerSize;
    struct Buffer {
        uint8_t* data;
        int size;
    };
    std::deque<Buffer> datas;
    uint32_t total;
#ifdef OS_Darwin
    VDADecoder decoder;
#endif
    uint8_t* avBuffer;
    int avBufferSize;
    AVIOContext* avCtx;
    AVFormatContext* fmtCtx;

    uint8_t* frame;
    int framePos, frameSize;

    static int readFunction(void* opaque, uint8_t* buf, int buf_size);
};

int DecoderPrivate::readFunction(void* opaque, uint8_t* buf, int buf_size)
{
    DecoderPrivate* priv = static_cast<DecoderPrivate*>(opaque);
    const int sz = std::min(priv->frameSize - priv->framePos, buf_size);
    if (!sz)
        return 0;
    memcpy(buf, &priv->frame[priv->framePos], sz);
    priv->framePos += sz;
    return sz;
}

#ifdef OS_Darwin
static void decoderOutputCallback(void* decompressionOutputRefCon,
                                  CFDictionaryRef frameInfo,
                                  OSStatus status,
                                  uint32_t infoFlags,
                                  CVImageBufferRef imageBuffer)
{
    printf("got decoded frame\n");
}
#endif

Decoder::Decoder()
    : mPriv(new DecoderPrivate)
{
    mPriv->inited = false;
    mPriv->total = 0;
    mPriv->fmtCtx = 0;
}

Decoder::~Decoder()
{
    av_free(mPriv->avBuffer);
    if (mPriv->fmtCtx) {
        avformat_free_context(mPriv->fmtCtx);
    }
    av_free(mPriv->avCtx);
    delete mPriv;
}

bool Decoder::inited() const
{
    return mPriv->inited;
}

void Decoder::init(const uint8_t* extradata, int extrasize,
                   const uint8_t* sps, int spsSize,
                   const uint8_t* pps, int ppsSize)
{
    if (mPriv->inited)
        return;
    mPriv->inited = true;

    mPriv->avBufferSize = 8192;
    mPriv->avBuffer = reinterpret_cast<uint8_t*>(av_malloc(mPriv->avBufferSize));
    mPriv->avCtx = avio_alloc_context(mPriv->avBuffer, mPriv->avBufferSize, 0, mPriv, DecoderPrivate::readFunction, 0, 0);

    mPriv->header = reinterpret_cast<uint8_t*>(malloc(spsSize + 4 + ppsSize + 4 + 4));
    int sz = 0;
    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x1;
    memcpy(mPriv->header + sz, sps, spsSize);
    sz += spsSize;

    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x1;
    memcpy(mPriv->header + sz, pps, ppsSize);
    sz += ppsSize;

    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x0;
    mPriv->header[sz++] = 0x1;

    mPriv->headerSize = sz;

    const int inWidth = 1440;
    const int inHeight = 900;

#ifdef OS_Darwin
    OSStatus status;
    OSType inSourceFormat = 'avc1';

    CFMutableDictionaryRef decoderConfiguration = NULL;
    CFMutableDictionaryRef destinationImageBufferAttributes = NULL;
    CFDictionaryRef emptyDictionary;

    CFNumberRef height = NULL;
    CFNumberRef width= NULL;
    CFNumberRef sourceFormat = NULL;
    CFNumberRef pixelFormat = NULL;

    CFDataRef inAVCCData = CFDataCreate(kCFAllocatorDefault, extradata, extrasize);

    // create a CFDictionary describing the source material for decoder configuration
    decoderConfiguration = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                     4,
                                                     &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);

    height = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inHeight);
    width = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inWidth);
    sourceFormat = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inSourceFormat);

    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_Height, height);
    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_Width, width);
    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_SourceFormat, sourceFormat);
    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_avcCData, inAVCCData);

    // create a CFDictionary describing the wanted destination image buffer
    destinationImageBufferAttributes = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                 2,
                                                                 &kCFTypeDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks);

    OSType cvPixelFormatType = kCVPixelFormatType_422YpCbCr8;
    pixelFormat = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &cvPixelFormatType);
    emptyDictionary = CFDictionaryCreate(kCFAllocatorDefault, // our empty IOSurface properties dictionary
                                         NULL,
                                         NULL,
                                         0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(destinationImageBufferAttributes, kCVPixelBufferPixelFormatTypeKey, pixelFormat);
    CFDictionarySetValue(destinationImageBufferAttributes,
                         kCVPixelBufferIOSurfacePropertiesKey,
                         emptyDictionary);

    // create the hardware decoder object
    status = VDADecoderCreate(decoderConfiguration,
                              destinationImageBufferAttributes,
                              (VDADecoderOutputCallback*)decoderOutputCallback,
                              this,
                              &mPriv->decoder);

    if (kVDADecoderNoErr != status) {
        fprintf(stderr, "VDADecoderCreate failed. err: %d\n", status);
    }

    if (decoderConfiguration) CFRelease(decoderConfiguration);
    if (destinationImageBufferAttributes) CFRelease(destinationImageBufferAttributes);
    if (emptyDictionary) CFRelease(emptyDictionary);

    return;
#endif
}

#ifdef OS_Darwin
static inline CFDictionaryRef MakeDictionaryWithDisplayTime(int64_t inFrameDisplayTime)
{
    CFStringRef key = CFSTR("FrameDisplayTime");
    CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &inFrameDisplayTime);

    return CFDictionaryCreate(kCFAllocatorDefault,
                              (const void **)&key,
                              (const void **)&value,
                              1,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
}
#endif

void Decoder::decode(const char* data, int size)
{
    printf("decoding %d\n", size);
    assert(size > 2);

    if (size == 8) { // is this a header?
        const uint64_t hdr = *reinterpret_cast<const uint64_t*>(data);
        if ((hdr & 0xff00) >> 8 == 0xbeeffeed) { // yes!
            if (!mPriv->datas.empty()) { // we didn't get the entire previous block of data
                std::deque<DecoderPrivate::Buffer>::const_iterator it = mPriv->datas.begin();
                const std::deque<DecoderPrivate::Buffer>::const_iterator end = mPriv->datas.end();
                while (it != end) {
                    free(it->data);
                    ++it;
                }
                mPriv->datas.clear();
                mPriv->total = 0;
            }

            mPriv->total = hdr & 0xff;
            return;
        }
    }

    if (mPriv->total == 0) // haven't seen a header yet
        return;

#warning apply mPriv->header in front of the data?

    // make a copy of the data
    uint8_t* buf = new uint8_t[size];
    memcpy(buf, data, size);
    if (mPriv->total == 1) { // optimize for the case where the block only has one datagram
        mPriv->frame = buf;
        mPriv->framePos = 0;
        mPriv->frameSize = size;
    } else {
        DecoderPrivate::Buffer buffer = { buf, size };
        mPriv->datas.push_back(buffer);
    }

    if (mPriv->datas.size() < mPriv->total) // haven't gotten all datagrams yet
        return;

    assert(mPriv->total == mPriv->datas.size());

    if (mPriv->total > 1) {
        // reassemble!
        mPriv->frame = new uint8_t[mPriv->total];
        mPriv->framePos = 0;
        mPriv->frameSize = mPriv->total;
        mPriv->total = 0;
        buf = mPriv->frame;
        std::deque<DecoderPrivate::Buffer>::const_iterator it = mPriv->datas.begin();
        const std::deque<DecoderPrivate::Buffer>::const_iterator end = mPriv->datas.end();
        while (it != end) {
            memcpy(buf, it->data, it->size);
            buf += it->size;
            free(it->data);
            ++it;
        }
        mPriv->datas.clear();
    }

    if (!mPriv->fmtCtx) {
        mPriv->fmtCtx = avformat_alloc_context();
        mPriv->fmtCtx->pb = mPriv->avCtx;
        int ret = avformat_open_input(&mPriv->fmtCtx, "dummy", 0, 0);
        if (ret)
            printf("avformat open error %d\n", ret);
        ret = avformat_find_stream_info(mPriv->fmtCtx, 0);
        if (ret < 0)
            printf("avformat find stream error %d\n", ret);
    }

    AVPacket packet;
    const int ok = av_read_frame(mPriv->fmtCtx, &packet);
    printf("av_read_frame %d\n", ok);
    if (ok >= 0)
        av_free_packet(&packet);

#warning update mPriv->frame wrt framePos? if not, it needs to be freed

    return;

#ifdef OS_Darwin
    CFDataRef frameData = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(mPriv->frame), mPriv->frameSize);

    CFDictionaryRef frameInfo = NULL;
    OSStatus status = kVDADecoderNoErr;

    // create a dictionary containg some information about the frame being decoded
    // in this case, we pass in the display time aquired from the stream
    frameInfo = MakeDictionaryWithDisplayTime(0 /*inFrameDisplayTime*/);

    // ask the hardware to decode our frame, frameInfo will be retained and pased back to us
    // in the output callback for this frame
    status = VDADecoderDecode(mPriv->decoder, 0, frameData, frameInfo);
    if (kVDADecoderNoErr != status) {
        fprintf(stderr, "VDADecoderDecode failed. err: %d\n", status);
    }

    // the dictionary passed into decode is retained by the framework so
    // make sure to release it here
    CFRelease(frameInfo);

    CFRelease(frameData);
#endif
}
