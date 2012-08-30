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
{
    mInited = false;
}

void Decoder::init(const uint8_t* extradata, int extrasize)
{
    if (mInited)
        return;
    mInited = true;

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
#ifdef OS_Darwin
    CFDataRef frameData = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data), size);

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
