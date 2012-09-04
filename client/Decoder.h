#ifndef DECODER_H
#define DECODER_H

#include <cstdint>
#include <string>

class DecoderPrivate;

class Decoder
{
public:
    Decoder();
    ~Decoder();

    void init(int width, int height, const std::string& extra);
    bool inited() const;

    void decode(const char* data, int size);

private:
    DecoderPrivate* mPriv;
};

#endif
