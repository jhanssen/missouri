#include "DWMCapturer.h"
#include <stdio.h>
#include <assert.h>

enum DWM_TMP_VALUES {
    DWM_TNP_RECTDESTINATION = 0x00000001,
    DWM_TNP_RECTSOURCE = 0x00000002,
    DWM_TNP_OPACITY = 0x00000004,
    DWM_TNP_VISIBLE = 0x00000008,
    DWM_TNP_SOURCECLIENTAREAONLY = 0x00000010
};

typedef HRESULT DWMAPI;
typedef HANDLE HTHUMBNAIL;
typedef HTHUMBNAIL* PHTHUMBNAIL;

typedef struct _DWM_THUMBNAIL_PROPERTIES
{
    DWORD dwFlags;
    RECT rcDestination;
    RECT rcSource;
    BYTE opacity;
    BOOL fVisible;
    BOOL fSourceClientAreaOnly;
} DWM_THUMBNAIL_PROPERTIES, *PDWM_THUMBNAIL_PROPERTIES;

typedef DWMAPI (WINAPI * DwmRegisterThumbnail_t)(HWND hwndDestination, HWND hwndSource, /*out*/ PHTHUMBNAIL phThumbnailId);
typedef DWMAPI (WINAPI * DwmUpdateThumbnailProperties_t)(HTHUMBNAIL hThumbnailId, const DWM_THUMBNAIL_PROPERTIES *ptnProperties);
typedef DWMAPI (WINAPI * DwmUnregisterThumbnail_t)(HTHUMBNAIL hThumbnailId);

DWMAPI DwmRegisterThumbnail(HWND hwndDestination, HWND hwndSource, /*out*/ PHTHUMBNAIL phThumbnailId)
{
    DWMAPI ret = ERROR_MOD_NOT_FOUND;
    HMODULE shell = LoadLibraryA("dwmapi.dll");
    if (shell) {
        DwmRegisterThumbnail_t register_thumbnail = reinterpret_cast<DwmRegisterThumbnail_t>(GetProcAddress(shell, "DwmRegisterThumbnail"));
        ret = register_thumbnail(hwndDestination, hwndSource, phThumbnailId);
        FreeLibrary(shell);
    }
    return ret;
}

DWMAPI DwmUpdateThumbnailProperties(HTHUMBNAIL hThumbnailId, const DWM_THUMBNAIL_PROPERTIES *ptnProperties)
{
    DWMAPI ret = ERROR_MOD_NOT_FOUND;
    HMODULE shell = LoadLibraryA("dwmapi.dll");
    if (shell) {
        DwmUpdateThumbnailProperties_t update_thumbnail_properties = reinterpret_cast<DwmUpdateThumbnailProperties_t>(GetProcAddress(shell, "DwmUpdateThumbnailProperties"));  
        ret = update_thumbnail_properties(hThumbnailId, ptnProperties);
        FreeLibrary(shell);
    }
    return ret;
}

DWMAPI DwmUnregisterThumbnail(HTHUMBNAIL hThumbnailId)
{
    DWMAPI ret = ERROR_MOD_NOT_FOUND;
    HMODULE shell = LoadLibraryA("dwmapi.dll");
    if (shell) {
        DwmUnregisterThumbnail_t unregister_thumbnail = reinterpret_cast<DwmUnregisterThumbnail_t>(GetProcAddress(shell, "DwmUnregisterThumbnail"));  
        ret = unregister_thumbnail(hThumbnailId);
        FreeLibrary(shell);
    }
    return ret;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

DWMCapturer::DWMCapturer()
    : mDest(NULL), mThumb(NULL)
{
    pthread_create(&mThread, 0, run, this);
}

DWMCapturer::~DWMCapturer()
{
    if (mDest) {
    }
}

void* DWMCapturer::run(void* arg)
{
    DWMCapturer* cap = static_cast<DWMCapturer*>(arg);

    //cap->mScreen = WindowFromDC(GetDC(NULL));
    cap->mScreen = FindWindowA(NULL, "name");
    if (!cap->mScreen) {
        printf("unable to get window for screen\n");
        return 0;
    }

    RECT screenRect;
    GetClientRect(cap->mScreen, &screenRect);

    cap->mWidth = screenRect.right - screenRect.left;
    cap->mHeight = screenRect.bottom - screenRect.top;
    printf("w %d, h %d\n", cap->mWidth, cap->mHeight);
    
    HINSTANCE inst = GetModuleHandle(NULL);
    const char* class_name = "DWMCAPTURE_CLASS";
    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = WndProc;
    wx.hInstance = inst;
    wx.lpszClassName = class_name;
    if (RegisterClassEx(&wx)) {
        cap->mDest = CreateWindowEx(0, class_name, "dwmcapture", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, cap->mWidth, cap->mHeight, /*HWND_MESSAGE*/ NULL, NULL, inst, NULL);
        printf("created window %p\n", cap->mDest);
        ShowWindow(cap->mDest, SW_SHOWNORMAL);
        UpdateWindow(cap->mDest);

        HRESULT res = DwmRegisterThumbnail(cap->mDest, cap->mScreen, &cap->mThumb);
        if (SUCCEEDED(res)) {
            printf("successfully registered thumbnail\n");
            DWM_THUMBNAIL_PROPERTIES prop;
            prop.rcDestination = { 0, 0, static_cast<long int>(cap->mWidth), static_cast<long int>(cap->mHeight) };
            prop.fVisible = TRUE;
            prop.fSourceClientAreaOnly = FALSE;
            prop.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_SOURCECLIENTAREAONLY;
            res = DwmUpdateThumbnailProperties(cap->mThumb, &prop);
            if (SUCCEEDED(res)) {
                printf("successfully updated thumbnail properties\n");
                cap->mDestDC = GetDC(cap->mDest);
                cap->mBmpDC = CreateCompatibleDC(cap->mDestDC);
                cap->mBmp = CreateCompatibleBitmap(cap->mDestDC, cap->mWidth, cap->mHeight);
                SelectObject(cap->mBmpDC, cap->mBmp);
            }
        }
    }

    cap->mBminfo.bmiHeader.biBitCount = 24;
    cap->mBminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    cap->mBminfo.bmiHeader.biCompression = BI_RGB;
    cap->mBminfo.bmiHeader.biPlanes = 1;
    cap->mBminfo.bmiHeader.biWidth = cap->mWidth;
    cap->mBminfo.bmiHeader.biHeight = -((int32_t) cap->mHeight);
    cap->mBminfo.bmiHeader.biSizeImage = cap->mWidth * 3 * cap->mHeight; // must be DWORD aligned (this assumes width * height is divisible by 4)
    cap->mBminfo.bmiHeader.biXPelsPerMeter = 0;
    cap->mBminfo.bmiHeader.biYPelsPerMeter = 0;
    cap->mBminfo.bmiHeader.biClrUsed = 0;
    cap->mBminfo.bmiHeader.biClrImportant = 0;
	
    cap->mBmpBuffer = new uint8_t[cap->mBminfo.bmiHeader.biSizeImage];

    if (cap->mDestDC) {
        MSG Msg;
        while(GetMessage(&Msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
    }
    return 0;
}

bool DWMCapturer::Capture()
{
#warning need to query the thread and get data back
    return true;
    if (!mDest)
        return false;
    int ret = BitBlt(mBmpDC, 0, 0, mWidth, mHeight, mDestDC, 0, 0, SRCCOPY);
    assert(ret);
    ret = GetDIBits(mBmpDC, mBmp, 0, mHeight, mBmpBuffer, &mBminfo, DIB_RGB_COLORS);
    assert(ret);
    return true;
}
