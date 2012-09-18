#include <iostream>
#include <fstream>

#include <cassert>

#include "GDICapturer.h"

GDICapturer::GDICapturer()
{
    Init();
}

GDICapturer::~GDICapturer()
{
}

void GDICapturer::Init()
{
    mScreen = GetDC(NULL);
    mTarget = CreateCompatibleDC(mScreen);

    mWidth = GetSystemMetrics(SM_CXSCREEN);
    mHeight = GetSystemMetrics(SM_CYSCREEN);

    mBmp = CreateCompatibleBitmap(mScreen, mWidth, mHeight);

    SelectObject(mTarget, mBmp);
	
    int ret = 0;
	
    mBminfo.bmiHeader.biBitCount = 24;
    mBminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    mBminfo.bmiHeader.biCompression = BI_RGB;
    mBminfo.bmiHeader.biPlanes = 1;
    mBminfo.bmiHeader.biWidth = mWidth;
    mBminfo.bmiHeader.biHeight = -((int32_t) mHeight);
    mBminfo.bmiHeader.biSizeImage = mWidth * 3 * mHeight; // must be DWORD aligned (this assumes width * height is divisible by 4)
    mBminfo.bmiHeader.biXPelsPerMeter = 0;
    mBminfo.bmiHeader.biYPelsPerMeter = 0;
    mBminfo.bmiHeader.biClrUsed = 0;
    mBminfo.bmiHeader.biClrImportant = 0;
	
    mBmpBuffer = new uint8_t[mBminfo.bmiHeader.biSizeImage];
}

bool GDICapturer::Capture()
{
    int ret = 0;
    ret = BitBlt(mTarget, 0, 0, mWidth, mHeight, mScreen, 0, 0, SRCCOPY);
    assert(ret);

    // add the cursor
    CURSORINFO ci;
	
    ci.cbSize = sizeof(CURSORINFO);
    GetCursorInfo(&ci);
    HCURSOR hcur = ci.hCursor;

    POINT cursor_coord;
    GetCursorPos(&cursor_coord);

    ICONINFO icon_info;
    ret = GetIconInfo(hcur, &icon_info);
	
    if (ret) // if there is an icon, draw it
    {
        cursor_coord.x -= icon_info.xHotspot;
        cursor_coord.y -= icon_info.yHotspot;

        DrawIcon(mTarget, cursor_coord.x, cursor_coord.y, hcur);

        DeleteObject(icon_info.hbmColor);
        DeleteObject(icon_info.hbmMask);
    }

    ret = GetDIBits(mTarget, mBmp, 0, mHeight, mBmpBuffer, &mBminfo, DIB_RGB_COLORS);
    assert(ret);

    return true;
}

bool GDICapturer::CaptureBmp(const std::string& name)
{
    int ret = 0;
    ret = BitBlt(mTarget, 0, 0, mWidth, mHeight, mScreen, 0, 0, SRCCOPY | CAPTUREBLT);
    assert(ret);

    // add the cursor
	
    CURSORINFO ci;
	
    ci.cbSize = sizeof(CURSORINFO);
    GetCursorInfo(&ci);
    HCURSOR hcur = ci.hCursor;

    POINT cursor_coord;
    GetCursorPos(&cursor_coord);

    ICONINFO icon_info;
    ret = GetIconInfo(hcur, &icon_info);
    assert(ret);

    cursor_coord.x -= icon_info.xHotspot;
    cursor_coord.y -= icon_info.yHotspot;

    DrawIcon(mTarget, cursor_coord.x, cursor_coord.y, hcur);
	
    ret = GetDIBits(mTarget, mBmp, 0, mHeight, mBmpBuffer, &mBminfo, DIB_RGB_COLORS);
    assert(ret);
	
    BITMAPFILEHEADER fheader;
    fheader.bfType = 0x4D42;
    fheader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + mBminfo.bmiHeader.biSizeImage;
    fheader.bfReserved1 = 0;
    fheader.bfReserved2 = 0;
    fheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	
    uint8_t *buf = new uint8_t[1024*4096];

    std::ofstream bmp_file(name.c_str(), std::ios_base::binary);
    bmp_file.rdbuf()->pubsetbuf((char *) buf, 1024*4096);
    bmp_file.write((const char *) &fheader, sizeof(fheader));
    bmp_file.write((const char *) &(mBminfo.bmiHeader), sizeof(mBminfo.bmiHeader));
    bmp_file.write((const char *) mBmpBuffer, mBminfo.bmiHeader.biSizeImage);
    bmp_file.close();

    return true;
}
