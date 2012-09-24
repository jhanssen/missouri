#ifndef WIN8CAPTURER_H
#define WIN8CAPTURER_H

#include <cstdint>
#include <string>
#include <Windows.h>

class Win8CapturerPrivate;

class Win8Capturer
{
public:
    Win8Capturer();
    ~Win8Capturer();

    bool Capture();
    uint8_t *GetBuffer() { return mBmpBuffer; }

    int32_t GetWidth() { return mWidth; }
    int32_t GetHeight() { return mHeight; }

private:
    uint32_t mWidth;
    uint32_t mHeight;

    uint8_t *mBmpBuffer;

    Win8CapturerPrivate* mPriv;
};

#endif // WIN8CAPTURER_H
