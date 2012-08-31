#include "Decoder.h"
#include <stdio.h>

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
    : mInited(false), mTotal(0)
{
}

void Decoder::init(const uint8_t* extradata, int extrasize,
                   const uint8_t* sps, int spsSize,
                   const uint8_t* pps, int ppsSize)
{
    if (mInited)
        return;
    mInited = true;

    mHeader = reinterpret_cast<uint8_t*>(malloc(spsSize + 4 + ppsSize + 4 + 4));
    int sz = 0;
    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x1;
    memcpy(mHeader + sz, sps, spsSize);
    sz += spsSize;

    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x1;
    memcpy(mHeader + sz, pps, ppsSize);
    sz += ppsSize;

    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x0;
    mHeader[sz++] = 0x1;

    mHeaderSize = sz;

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
                              &mDecoder);

    if (kVDADecoderNoErr != status) {
        fprintf(stderr, "VDADecoderCreate failed. err: %d\n", status);
    }

    if (decoderConfiguration) CFRelease(decoderConfiguration);
    if (destinationImageBufferAttributes) CFRelease(destinationImageBufferAttributes);
    if (emptyDictionary) CFRelease(emptyDictionary);

    return;
#endif
}

Decoder::~Decoder()
{
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
            assert(!mTotal && mDatas.empty());
            mTotal = hdr & 0xff;
            return;
        }
    }

    char* frame = 0;
    uint32_t frameSize = 0;

    char* buf = new char[size];
    memcpy(buf, data, size);
    if (mTotal == 1) {
        frame = buf;
        frameSize = size;
    } else {
        Buffer buffer = { buf, size };
        mDatas.push_back(buffer);
    }

    if (mTotal > 1 && mTotal == mDatas.size()) {
        // reassemble!
        frame = new char[mTotal];
        frameSize = mTotal;
        mTotal = 0;
        buf = frame;
        std::deque<Buffer>::const_iterator it = mDatas.begin();
        const std::deque<Buffer>::const_iterator end = mDatas.end();
        while (it != end) {
            memcpy(buf, it->data, it->size);
            buf += it->size;
            free(it->data);
            ++it;
        }
        mDatas.clear();
    }

#ifdef OS_Darwin
    CFDataRef frameData = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(frame), frameSize);

    CFDictionaryRef frameInfo = NULL;
    OSStatus status = kVDADecoderNoErr;

    // create a dictionary containg some information about the frame being decoded
    // in this case, we pass in the display time aquired from the stream
    frameInfo = MakeDictionaryWithDisplayTime(0 /*inFrameDisplayTime*/);

    // ask the hardware to decode our frame, frameInfo will be retained and pased back to us
    // in the output callback for this frame
    status = VDADecoderDecode(mDecoder, 0, frameData, frameInfo);
    if (kVDADecoderNoErr != status) {
        fprintf(stderr, "VDADecoderDecode failed. err: %d\n", status);
    }

    // the dictionary passed into decode is retained by the framework so
    // make sure to release it here
    CFRelease(frameInfo);

    CFRelease(frameData);
#endif
}
