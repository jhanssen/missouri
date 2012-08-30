#ifndef ENCODER_H
#define ENCODER_H

#include <cstdint>

class EncoderPrivate;

class Encoder
{
public:
    Encoder(const uint8_t* buffer, int32_t width, int32_t height, int32_t size);
    ~Encoder();

    void encode();

    const uint8_t* outputBuffer() const;
    uint32_t outputSize() const;

    void getSps(uint8_t** payload, int* size);
    void getPps(uint8_t** payload, int* size);

private:
    EncoderPrivate* mPriv;
};

#endif
