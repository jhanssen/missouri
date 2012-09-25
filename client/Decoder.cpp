#include "Decoder.h"
#include "main.h"
#include <stdio.h>
#include <assert.h>
#include <deque>
#ifdef OS_Darwin
# include <VDADecoder.h>
#endif

class DecoderPrivate
{
public:
    bool inited;
    struct Buffer {
        uint8_t* data;
        int size;
    };
    std::deque<Buffer> datas;
    uint32_t total;
#ifdef OS_Darwin
    VDADecoder decoder;
#endif

    uint8_t* frame;
    int framePos, frameSize;
};

#ifdef OS_Darwin
static void decoderOutputCallback(void* decompressionOutputRefCon,
                                  CFDictionaryRef frameInfo,
                                  OSStatus status,
                                  uint32_t infoFlags,
                                  CVImageBufferRef imageBuffer)
{
    if (imageBuffer) {
        postImage(imageBuffer);
        //printf("??got decoded frame\n");
    } else {
        //printf("??callback with no image!\n");
    }
}
#endif

Decoder::Decoder()
    : mPriv(new DecoderPrivate)
{
    mPriv->inited = false;
    mPriv->total = 0;
}

Decoder::~Decoder()
{
    delete mPriv;
}

bool Decoder::inited() const
{
    return mPriv->inited;
}

void Decoder::init(int iw, int ih, const std::string& extra)
{
    if (mPriv->inited)
        return;
    mPriv->inited = true;

#ifdef OS_Darwin
    OSStatus status;
    OSType inSourceFormat = 'avc1';

    CFMutableDictionaryRef decoderConfiguration = NULL;
    CFMutableDictionaryRef destinationImageBufferAttributes = NULL;
    CFDictionaryRef emptyDictionary;

    CFNumberRef height = NULL;
    CFNumberRef width = NULL;
    CFNumberRef sourceFormat = NULL;
    CFNumberRef pixelFormat = NULL;

    CFDataRef inAVCCData = CFDataCreate(kCFAllocatorDefault,
                                        reinterpret_cast<const uint8_t*>(extra.data()), extra.size());

    // create a CFDictionary describing the source material for decoder configuration
    decoderConfiguration = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                     4,
                                                     &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);

    height = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ih);
    width = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &iw);
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
    //printf("decoding %d\n", size);
    assert(size > 2);

    if (size == 8) { // is this a separator?
        /*
        printf("potential header ");
        for (int i = 0; i < 8; ++i) {
            printf("%02x ", reinterpret_cast<const unsigned char*>(data)[i]);
        }
        printf("\n");
        */
        const uint64_t hdr = *reinterpret_cast<const uint64_t*>(data);
        if (((hdr & 0xffffffff00000000LL) >> 32) == 0xbeeffeed) { // yes!const uint32_t hdr = *reinterpret_cast<const uint32_t*>(data);
            //printf("found header\n");
            const uint32_t nalCount = (hdr & 0x00000000ffffffffLL);
            if (!mPriv->datas.empty()) {
                // we got data

                // reassemble!
                mPriv->frame = new uint8_t[mPriv->total];
                mPriv->framePos = 0;
                mPriv->frameSize = mPriv->total;
                uint8_t* buf = mPriv->frame;
                int left = mPriv->datas.size() - nalCount;
                std::deque<DecoderPrivate::Buffer>::const_iterator it = mPriv->datas.begin();
                const std::deque<DecoderPrivate::Buffer>::const_iterator end = mPriv->datas.end();
                while (it != end) {
                    if (--left > 0) {
                        mPriv->frameSize -= it->size;
                        continue;
                    }
                    memcpy(buf, it->data, it->size);
                    buf += it->size;
                    free(it->data);
                    ++it;
                }
                mPriv->datas.clear();
                mPriv->total = 0;

                /*
                  for (int i = 0; i < 16; ++i)
                  printf("%02x ", mPriv->frame[i]);
                  printf("\nframe done\n");
                */

#ifdef OS_Darwin
                CFDataRef frameData = CFDataCreate(kCFAllocatorDefault, mPriv->frame, mPriv->frameSize);

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

                delete[] mPriv->frame;

                return;
            }
        }
    }

    // make a copy of the data
    uint8_t* buf = new uint8_t[size];
    memcpy(buf, data, size);

    DecoderPrivate::Buffer buffer = { buf, size };
    mPriv->total += size;
    mPriv->datas.push_back(buffer);
}
