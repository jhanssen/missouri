#include "Decoder.h"
#include <stdio.h>

static void decoderOutputCallback(void* decompressionOutputRefCon,
                                  CFDictionaryRef frameInfo,
                                  OSStatus status,
                                  uint32_t infoFlags,
                                  CVImageBufferRef imageBuffer)
{
}

Decoder::Decoder(const uint8_t* extradata, int extrasize)
{
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
                              mDecoder);

    if (kVDADecoderNoErr != status) {
        fprintf(stderr, "VDADecoderCreate failed. err: %d\n", status);
        mDecoder = 0;
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

void Decoder::decode(const char* data, int size)
{
    printf("decoding %d\n", size);
}
