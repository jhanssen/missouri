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
    
    DXGI_OUTPUT_DESC OutputDesc;
    HANDLE Thread;
    DWORD ThreadId;
    CRITICAL_SECTION Mutex;

    bool stopped;

    FrameCallback callback;
    void* userData;
};

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
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
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
        hr = data->Context->Map(target, subresource, D3D11_MAP_READ, 0, &mapdesc);
        if (FAILED(hr)) {
            printf("ID3D11DeviceContext::Map failed 0x%x\n", hr);
            break;
        }
        if (mapdesc.pData) {
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
