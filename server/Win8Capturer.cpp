#include "Win8Capturer.h"
#include <Win8Dupl.h>
#include <Windows.h>
#include <cassert>
#include <stdio.h>

class Win8CapturerPrivate
{
public:
    HMODULE lib;
    StartDuplicationPtr start;
    StopDuplicationPtr stop;
    DuplicationHandle handle;

    CRITICAL_SECTION mutex;
    void* data;
    int width, height, pitch;
    int prevsize;

    static void frameCallback(DuplicationHandle handle, void* data, int width, int height, int pitch, void* userData);
};

void Win8CapturerPrivate::frameCallback(DuplicationHandle handle, void* data, int width, int height, int pitch, void* userData)
{
    Win8CapturerPrivate* priv = static_cast<Win8CapturerPrivate*>(userData);
    //printf("frame callback %dx%d\n", width, height);
    EnterCriticalSection(&priv->mutex);
    if (priv->prevsize != (pitch * height)) {
        if (priv->data)
            free(priv->data);
        priv->prevsize = pitch * height;
        priv->data = malloc(priv->prevsize);
        priv->width = width;
        priv->height = height;
        priv->pitch = pitch;
    }
    memcpy(priv->data, data, priv->prevsize);
    LeaveCriticalSection(&priv->mutex);
}

Win8Capturer::Win8Capturer()
    : mPriv(new Win8CapturerPrivate)
{
    memset(mPriv, 0, sizeof(Win8CapturerPrivate));
    InitializeCriticalSection(&mPriv->mutex);

    mWidth = GetSystemMetrics(SM_CXSCREEN);
    mHeight = GetSystemMetrics(SM_CYSCREEN);

    mBmpBuffer = new uint8_t[(mWidth * mHeight) * 4];

    mPriv->lib = LoadLibraryA("..\\win8dupl\\Win8Dupl\\Release\\Win8Dupl.dll");
    if (!mPriv->lib) {
        printf("library load failure\n");
        return;
    }
    mPriv->start = reinterpret_cast<StartDuplicationPtr>(GetProcAddress(mPriv->lib, "StartDuplication"));
    mPriv->stop = reinterpret_cast<StopDuplicationPtr>(GetProcAddress(mPriv->lib, "StopDuplication"));
    if (!mPriv->start || !mPriv->stop) {
        printf("library resolve failure\n");
        FreeLibrary(mPriv->lib);
        mPriv->lib = 0;
        return;
    }
    const int ret = mPriv->start(&mPriv->handle, Win8CapturerPrivate::frameCallback, mPriv);
    if (ret != DUPL_SUCCESS) {
        printf("dupl failure %d\n", ret);
        FreeLibrary(mPriv->lib);
        mPriv->lib = 0;
    }
}

Win8Capturer::~Win8Capturer()
{
    if (mPriv->lib) {
        assert(mPriv->stop);
        if (mPriv->handle)
            mPriv->stop(mPriv->handle);
        FreeLibrary(mPriv->lib);
    }
    if (mPriv->data)
        free(mPriv->data);
    DeleteCriticalSection(&mPriv->mutex);
    delete mPriv;
    delete[] mBmpBuffer;
}

bool Win8Capturer::Capture()
{
    EnterCriticalSection(&mPriv->mutex);
    if (!mPriv->data) {
        LeaveCriticalSection(&mPriv->mutex);
        return false;
    }

    //printf("cap %dx%d vs %dx%d\n", mWidth, mHeight, mPriv->width, mPriv->height);

    uint8_t* dst = mBmpBuffer;
    uint8_t* src = reinterpret_cast<uint8_t*>(mPriv->data);
    const int h = mPriv->height;
    const int w = mPriv->width * 4;
    const int p = mPriv->pitch;
    if (p == w) {
        // more efficient
        memcpy(dst, src, w * h);
    } else {
        assert(p > w);
        for (int y = 0; y < h; ++y) {
            memcpy(dst, src, w);
            dst += w;
            src += p;
        }
    }
    LeaveCriticalSection(&mPriv->mutex);
    return true;
}
