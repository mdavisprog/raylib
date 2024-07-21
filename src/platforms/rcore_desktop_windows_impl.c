/**********************************************************************************************
*
*   rcore_desktop_windows_impl - Functions to implement Windows specific functionality to prevent
*                                name clashing with existing raylib functions.
*
*   PLATFORM: Windows
*
*   CONFIGURATION:
*
*   DEPENDENCIES:
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2024 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "rcore_desktop_windows_impl.h"

#ifndef UNICODE
#define UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define WND_CLASS_NAME L"raylib"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

typedef struct
{
    HWND handle;
    ID3D12Device9* device;
    IDXGIFactory7* factory;
    IDXGIAdapter1* adapter;
} PlatformData;

static PlatformData platform = { 0 };

static char* ToMultiByte(wchar_t* data)
{
    const int dataLength = (int)wcslen(data);
    const int length = WideCharToMultiByte(CP_ACP, 0, data, dataLength, NULL, 0, NULL, NULL);
    char* result = (char*)malloc(sizeof(char) * (length + 1));
    WideCharToMultiByte(CP_ACP, 0, data, dataLength, result, length, NULL, NULL);
    result[length] = 0;
    return result;
}

static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static BOOL EnumAdapter(UINT index, IDXGIFactory7* factory, IDXGIAdapter1** adapter)
{
    HRESULT Result = factory->lpVtbl->EnumAdapterByGpuPreference(factory, index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter1, (LPVOID*)&(*adapter));
    return SUCCEEDED(Result) ? TRUE : FALSE;
}

static BOOL IsValidAdapter(IDXGIAdapter1* adapter)
{
    DXGI_ADAPTER_DESC1 desc = { 0 };
    HRESULT result = adapter->lpVtbl->GetDesc1(adapter, &desc);

    if (FAILED(result))
    {
        printf("DIRECTX: Failed to retrieve description for adaptar!\n");
        return FALSE;
    }

    if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
    {
        return FALSE;
    }

    IUnknown* unknownAdapter = NULL;
    if (FAILED(adapter->lpVtbl->QueryInterface(adapter, &IID_IUnknown, (LPVOID*)&unknownAdapter)))
    {
        return FALSE;
    }

    result = D3D12CreateDevice(unknownAdapter, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device9, NULL);
    if (FAILED(result))
    {
        return FALSE;
    }

    return TRUE;
}

int Windows_Initialize()
{
    WNDCLASSEXW class = { 0 };
    class.cbSize = sizeof(class);
    class.style = CS_HREDRAW | CS_VREDRAW;
    class.lpszClassName = WND_CLASS_NAME;
    class.lpfnWndProc = WndProc;
    class.hInstance = GetModuleHandleW(NULL);
    class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    class.hIcon = NULL;
    class.hIconSm = NULL;
    class.hbrBackground = NULL;
    class.lpszMenuName = NULL;

    if (RegisterClassExW(&class) == 0)
    {
        printf("WINDOWS: Failed to register class!\n");
        return -1;
    }

    return 0;
}

void Windows_Close()
{
    DestroyWindow(platform.handle);
    UnregisterClassW(WND_CLASS_NAME, NULL);

    platform.handle = NULL;
}

int Windows_CreateWindow(const char* title, int width, int height)
{
    if (platform.handle != NULL)
    {
        return 0;
    }

    const int titleLength = (int)strlen(title);
    const int length = MultiByteToWideChar(CP_ACP, 0, title, titleLength, NULL, 0);
    wchar_t* wTitle = (wchar_t*)malloc(sizeof(wchar_t) * (length + 1));
    MultiByteToWideChar(CP_ACP, 0, title, titleLength, wTitle, length);
    wTitle[length] = 0;

    DWORD style = WS_OVERLAPPEDWINDOW;
    platform.handle = CreateWindowExW(0, WND_CLASS_NAME, wTitle, style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
    free(wTitle);

    if (platform.handle == NULL)
    {
        printf("WINDOWS: Failed to create window!\n");
        return -1;
    }

    return 0;
}

int DirectX_Initialize()
{
    IDXGIFactory7* factory = NULL;
    HRESULT result = CreateDXGIFactory2(0, &IID_IDXGIFactory7, (LPVOID*)&factory);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create DXGI Factory!\n");
        return -1;
    }

    IDXGIFactory7* queryFactory = NULL;
    result = factory->lpVtbl->QueryInterface(factory, &IID_IDXGIFactory7, (LPVOID*)&queryFactory);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to query factory interface!\n");
        return -1;
    }

    IDXGIAdapter1* adapter = NULL;
    for (UINT index = 0; EnumAdapter(index, queryFactory, &adapter); index++)
    {
        if (IsValidAdapter(adapter))
        {
            break;
        }
    }

    if (adapter == NULL)
    {
        for (UINT index = 0; SUCCEEDED(factory->lpVtbl->EnumAdapters1(factory, index, &adapter)); index++)
        {
            if (IsValidAdapter(adapter))
            {
                break;
            }
        }
    }

    queryFactory->lpVtbl->Release(queryFactory);
    platform.factory = factory;
    platform.adapter = adapter;

    IUnknown* unknownAdapter = NULL;
    if (FAILED(adapter->lpVtbl->QueryInterface(adapter, &IID_IUnknown, (LPVOID*)&unknownAdapter)))
    {
        printf("DIRECTX: Failed to query IUnknown for IDXGIAdapter!\n");
        return -1;
    }

    result = D3D12CreateDevice(unknownAdapter, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device9, (LPVOID*)&platform.device);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create device!\n");
        return -1;
    }

    DXGI_ADAPTER_DESC1 desc = { 0 };
    if (FAILED(platform.adapter->lpVtbl->GetDesc1(platform.adapter, &desc)))
    {
        printf("DIRECTX: Failed to retrieve adapter description!\n");
        return -1;
    }

    char* vendor = ToMultiByte(desc.Description);
    printf("DIRECTX: Device is %s.\n", vendor);
    free(vendor);

    return 0;
}

void DirectX_Shutdown()
{
    platform.adapter->lpVtbl->Release(platform.adapter);
    platform.factory->lpVtbl->Release(platform.factory);
    platform.device->lpVtbl->Release(platform.device);
}
