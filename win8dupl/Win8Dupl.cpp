// Win8Dupl.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Win8Dupl.h"
#include <stdio.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <sal.h>
#include <new>
#include <assert.h>
#include <DirectXMath.h>

struct HandleData
{
    ID3D11Device* Device;
    ID3D11DeviceContext* Context;
    IDXGIOutputDuplication* DeskDupl;

    uint8_t* PointerBuffer;
    unsigned int PointerSize;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO PointerShape;
    DXGI_OUTDUPL_POINTER_POSITION PointerPosition;
    
    DXGI_OUTPUT_DESC OutputDesc;
    HANDLE Thread;
    DWORD ThreadId;
    CRITICAL_SECTION Mutex;

    bool stopped;

    FrameCallback callback;
    void* userData;
};

// Courtesy of http://code.google.com/p/libyuv/, BSD licensed
__declspec(naked) __declspec(align(16))
void ARGBBlendRow_SSE2(const uint8_t* src_argb0, const uint8_t* src_argb1,
                       uint8_t* dst_argb, int width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]   // src_argb0
    mov        esi, [esp + 4 + 8]   // src_argb1
    mov        edx, [esp + 4 + 12]  // dst_argb
    mov        ecx, [esp + 4 + 16]  // width
    pcmpeqb    xmm7, xmm7       // generate constant 1
    psrlw      xmm7, 15
    pcmpeqb    xmm6, xmm6       // generate mask 0x00ff00ff
    psrlw      xmm6, 8
    pcmpeqb    xmm5, xmm5       // generate mask 0xff00ff00
    psllw      xmm5, 8
    pcmpeqb    xmm4, xmm4       // generate mask 0xff000000
    pslld      xmm4, 24

    sub        ecx, 1
    je         convertloop1     // only 1 pixel?
    jl         convertloop1b

    // 1 pixel loop until destination pointer is aligned.
  alignloop1:
    test       edx, 15          // aligned?
    je         alignloop1b
    movd       xmm3, [eax]
    lea        eax, [eax + 4]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movd       xmm2, [esi]      // _r_b
    psrlw      xmm3, 8          // alpha
    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
    pshuflw    xmm3, xmm3,0F5h
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movd       xmm1, [esi]      // _a_g
    lea        esi, [esi + 4]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 1
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    jge        alignloop1

  alignloop1b:
    add        ecx, 1 - 4
    jl         convertloop4b

    // 4 pixel loop.
  convertloop4:
    movdqu     xmm3, [eax]      // src argb
    lea        eax, [eax + 16]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movdqu     xmm2, [esi]      // _r_b
    psrlw      xmm3, 8          // alpha
    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
    pshuflw    xmm3, xmm3,0F5h
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movdqu     xmm1, [esi]      // _a_g
    lea        esi, [esi + 16]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jge        convertloop4

  convertloop4b:
    add        ecx, 4 - 1
    jl         convertloop1b

    // 1 pixel loop.
  convertloop1:
    movd       xmm3, [eax]      // src argb
    lea        eax, [eax + 4]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movd       xmm2, [esi]      // _r_b
    psrlw      xmm3, 8          // alpha
    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
    pshuflw    xmm3, xmm3,0F5h
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movd       xmm1, [esi]      // _a_g
    lea        esi, [esi + 4]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 1
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    jge        convertloop1

  convertloop1b:
    pop        esi
    ret
  }
}

static DWORD WINAPI DuplThread(LPVOID lpParameter)
{
    HandleData* data = reinterpret_cast<HandleData*>(lpParameter);

    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D* target = nullptr;
    ID3D11Texture2D* source = nullptr;
    IDXGIResource* DesktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;

    HRESULT hr;

    const unsigned int subresource = D3D11CalcSubresource(0, 0, 0);

    /*
    HDESK CurrentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
    if (!CurrentDesktop) {
        fprintf(stderr, "Could not open desktop\n");
        return 0;
    }
    const bool DesktopAttached = SetThreadDesktop(CurrentDesktop) != 0;
    CloseDesktop(CurrentDesktop);
    CurrentDesktop = nullptr;
    if (!DesktopAttached) {
        fprintf(stderr, "Could not attach desktop\n");
        return 0;
    }
    */

    for (;;) {
        EnterCriticalSection(&data->Mutex);
        if (data->stopped) {
            LeaveCriticalSection(&data->Mutex);
            break;
        }
        LeaveCriticalSection(&data->Mutex);

        // Get new frame
        hr = data->DeskDupl->AcquireNextFrame(1000, &FrameInfo, &DesktopResource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue;
        }

        if (FAILED(hr)) {
            fprintf(stderr, "AcquireNextFrame failed 0x%x\n", hr);
            break;
        }

        hr = DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&source));
        DesktopResource->Release();
        DesktopResource = nullptr;

        if (FAILED(hr)) {
            fprintf(stderr, "DesktopResource::QueryInterface for texture failed 0x%x\n", hr);
            break;
        }

        source->GetDesc(&desc);
        if (!target) {
            desc.BindFlags = 0;
            desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.Usage = D3D11_USAGE_STAGING;
            hr = data->Device->CreateTexture2D(&desc, NULL, &target);
            if (FAILED(hr)) {
                fprintf(stderr, "CreateTexture2D failed 0x%x\n", hr);
                break;
            }
            assert(target);
        }

        data->Context->CopyResource(target, source);

        D3D11_MAPPED_SUBRESOURCE mapdesc;
        hr = data->Context->Map(target, subresource, D3D11_MAP_READ_WRITE, 0, &mapdesc);
        if (FAILED(hr)) {
            printf("ID3D11DeviceContext::Map failed 0x%x\n", hr);
            break;
        }
        if (mapdesc.pData) {
            if (FrameInfo.LastMouseUpdateTime.QuadPart != 0 && FrameInfo.PointerPosition.Visible) {
                // handle mouse information
                if (FrameInfo.PointerShapeBufferSize) {
                    if (data->PointerSize < FrameInfo.PointerShapeBufferSize) {
                        data->PointerSize = FrameInfo.PointerShapeBufferSize;
                        delete[] data->PointerBuffer;
                        data->PointerBuffer = new uint8_t[data->PointerSize];
                    }
                    UINT BufferSizeRequired;
                    hr = data->DeskDupl->GetFramePointerShape(FrameInfo.PointerShapeBufferSize, data->PointerBuffer, &BufferSizeRequired, &data->PointerShape);
                    if (FAILED(hr)) {
                        fprintf(stderr, "GetFramePointerShape failed 0x%x\n", hr);
                        break;
                    }
                }
                data->PointerPosition = FrameInfo.PointerPosition;
            }
            if (data->PointerShape.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) {
                // Not supporting mono and masked pointers at the moment
                //printf("Drawing pointer at %d %d\n", data->PointerPosition.Position.x, data->PointerPosition.Position.y);
                const int ptrx = data->PointerPosition.Position.x + data->PointerShape.HotSpot.x;
                const int ptry = data->PointerPosition.Position.y + data->PointerShape.HotSpot.y;
                uint8_t* ptr = data->PointerBuffer;
                uint8_t* dst;
                // ### Should really do the blending on the GPU (Using DirectX) rather than using SSE2 on the CPU
                const int ptrw = min(data->PointerShape.Width, desc.Width - ptrx);
                for (unsigned int y = 0; y < data->PointerShape.Height; ++y) {
                    if (y + ptry >= desc.Height)
                        break;
                    dst = static_cast<uint8_t*>(mapdesc.pData) + (((y + ptry) * mapdesc.RowPitch) + (ptrx * 4));
                    //memcpy(dst, ptr, data->PointerShape.Width * 4);
                    ARGBBlendRow_SSE2(ptr, dst, dst, ptrw);
                    ptr += data->PointerShape.Pitch;
                }
            }

            data->callback(data, mapdesc.pData, desc.Width, desc.Height, mapdesc.RowPitch, data->userData);
        }

        data->Context->Unmap(target, subresource);
        data->DeskDupl->ReleaseFrame();
        source->Release();
    }

    if (target)
        target->Release();

    return 0;
}

static void cleanup(HandleData* data)
{
    if (data->Thread) {
        EnterCriticalSection(&data->Mutex);
        data->stopped = true;
        LeaveCriticalSection(&data->Mutex);
        WaitForSingleObject(data->Thread, INFINITE);
        CloseHandle(data->Thread);
        DeleteCriticalSection(&data->Mutex);
    }
    if (data->DeskDupl)
        data->DeskDupl->Release();
    if (data->Device)
        data->Device->Release();
    if (data->Context)
        data->Context->Release();
    delete[] data->PointerBuffer;
    delete data;
}

extern "C" WIN8DUPL_API int StartDuplication(DuplicationHandle* handle, FrameCallback callback, void* userData)
{
    HandleData* data = new HandleData;
    memset(data, 0, sizeof(HandleData));
    data->callback = callback;
    data->userData = userData;
    *handle = data;

    const int Output = 0;

    HRESULT hr = S_OK;

    // Driver types supported
    D3D_DRIVER_TYPE DriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

    // Feature levels supported
    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

    D3D_FEATURE_LEVEL FeatureLevel;

    // Create device
    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex) {
        hr = D3D11CreateDevice(nullptr, DriverTypes[DriverTypeIndex], nullptr, 0, FeatureLevels, NumFeatureLevels,
                               D3D11_SDK_VERSION, &data->Device, &FeatureLevel, &data->Context);
        if (SUCCEEDED(hr)) {
            // Device creation success, no need to loop anymore
            break;
        }
    }
    if (FAILED(hr)) {
        cleanup(data);
        return DUPL_D3D_DEVICE_CREATION;
    }

    IDXGIDevice* DxgiDevice = nullptr;
    hr = data->Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
    if (FAILED(hr)) {
        cleanup(data);
        return DUPL_DXGI_DEVICE_QI;
    }

    IDXGIAdapter* DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
    DxgiDevice->Release();
    DxgiDevice = nullptr;
    if (FAILED(hr)) {
        cleanup(data);
        return DUPL_DXGI_PARENT;
    }

    // Get output
    IDXGIOutput* DxgiOutput = nullptr;
    hr = DxgiAdapter->EnumOutputs(Output, &DxgiOutput);
    DxgiAdapter->Release();
    DxgiAdapter = nullptr;
    if (FAILED(hr)) {
        cleanup(data);
        return DUPL_ENUM_OUTPUTS;
    }

    DxgiOutput->GetDesc(&data->OutputDesc);

    // QI for Output 1
    IDXGIOutput1* DxgiOutput1 = nullptr;
    hr = DxgiOutput->QueryInterface(__uuidof(DxgiOutput1), reinterpret_cast<void**>(&DxgiOutput1));
    DxgiOutput->Release();
    DxgiOutput = nullptr;
    if (FAILED(hr)) {
        cleanup(data);
        return DUPL_DXGI_OUTPUT1_QI;
    }

    // Create desktop duplication
    hr = DxgiOutput1->DuplicateOutput(data->Device, &data->DeskDupl);
    DxgiOutput1->Release();
    DxgiOutput1 = nullptr;
    if (FAILED(hr)) {
        cleanup(data);
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            return DUPL_DUPLICATE_MAXIMUM;
        }
        return DUPL_DUPLICATE_FAILURE;
    }

    InitializeCriticalSection(&data->Mutex);
    data->Thread = CreateThread(0, 0, DuplThread, data, 0, &data->ThreadId);

    return DUPL_SUCCESS;
}

extern "C" WIN8DUPL_API void StopDuplication(DuplicationHandle handle)
{
    HandleData* data = static_cast<HandleData*>(handle);
    cleanup(data);
}
