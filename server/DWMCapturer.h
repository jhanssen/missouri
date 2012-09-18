#ifndef DWMCAPTURER
#define DWMCAPTURER

#include <cstdint>
#include <pthread.h>
#include <Windows.h>
#include <Winuser.h>

class DWMCapturer
{
public:
    DWMCapturer();
    ~DWMCapturer();

    bool Capture();

    uint8_t *GetBuffer() { return mBmpBuffer; }

    int32_t GetWidth() { return mWidth; }
    int32_t GetHeight() { return mHeight; }
    int32_t GetImageSize() { return mBminfo.bmiHeader.biSizeImage; }

private:
    static void* run(void*);

private:
    HWND mScreen;
    HWND mDest;
    HDC mDestDC;
    HDC mBmpDC;
    HBITMAP mBmp;
    HANDLE mThumb;

    uint32_t mWidth;
    uint32_t mHeight;

    uint8_t *mBmpBuffer;

    BITMAPINFO mBminfo;

    pthread_t mThread;
};

#endif
