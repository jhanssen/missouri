#ifndef WIN8DUPL_H
#define WIN8DUPL_H

#ifdef WIN8DUPL_EXPORTS
#define WIN8DUPL_API __declspec(dllexport)
#else
#define WIN8DUPL_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* DuplicationHandle;
typedef void (*FrameCallback)(DuplicationHandle handle, void* data, int width, int height, int pitch, void* userData);

enum {
    DUPL_SUCCESS,
    DUPL_D3D_DEVICE_CREATION,
    DUPL_DXGI_DEVICE_QI,
    DUPL_DXGI_PARENT,
    DUPL_ENUM_OUTPUTS,
    DUPL_DXGI_OUTPUT1_QI,
    DUPL_DUPLICATE_MAXIMUM,
    DUPL_DUPLICATE_FAILURE
};

typedef int (*StartDuplicationPtr)(DuplicationHandle*, FrameCallback, void*);
typedef void (*StopDuplicationPtr)(DuplicationHandle);

WIN8DUPL_API int StartDuplication(DuplicationHandle* handle, FrameCallback callback, void* userData);
WIN8DUPL_API void StopDuplication(DuplicationHandle handle);

#ifdef __cplusplus
}
#endif

#endif // WIN8DUPL_H
