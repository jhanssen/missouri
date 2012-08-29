#ifndef GDICAPTURER_H
#define GDICAPTURER_H

#include <cstdint>
#include <string>
#include <Windows.h>

class GDICapturer
{
public:
    GDICapturer();
    ~GDICapturer();

    bool Init();
    bool Capture();
    bool CaptureBmp(const std::string& name);
    uint8_t *GetBuffer() { return mBmpBuffer; }

    // only valid after init
    int32_t GetWidth() { return mWidth; }
    int32_t GetHeight() { return mHeight; }
    int32_t GetImageSize() { return mBminfo.bmiHeader.biSizeImage; }

private:
    HDC mScreen;
    HDC mTarget;
    HBITMAP mBmp;

    uint32_t mWidth;
    uint32_t mHeight;

    uint8_t *mBmpBuffer;

    BITMAPINFO mBminfo;
};

#endif // GDICAPTURER_H
