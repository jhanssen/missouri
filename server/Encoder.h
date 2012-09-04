#ifndef ENCODER_H
#define ENCODER_H

#include <cstdint>
#include "Host.h"

class EncoderPrivate;

class Encoder
{
public:
    Encoder(const uint8_t* buffer, int32_t width, int32_t height, int32_t size);
    ~Encoder();

    void ref(const Host& host, int width, int height);
    void deref(const Host& host);

    void encode();

    const uint8_t* outputBuffer() const;
    uint32_t outputSize() const;

    int encodedWidth() const;
    int encodedHeight() const;

    void getSps(uint8_t** payload, int* size);
    void getPps(uint8_t** payload, int* size);
    int headerSize() const;

private:
    void init();
    void deinit();

private:
    EncoderPrivate* mPriv;
};

#endif
