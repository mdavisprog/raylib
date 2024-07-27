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

#define DXRELEASE(object) if (object != NULL) { object->lpVtbl->Release(object); object = NULL; }

typedef struct
{
    ID3D12DescriptorHeap* descriptorHeap;
    UINT heapSize;
} DescriptorHeap;

typedef struct
{
    HWND handle;
    ID3D12Device9* device;
    IDXGIFactory7* factory;
    IDXGIAdapter1* adapter;
    ID3D12CommandQueue* commandQueue;
    ID3D12CommandAllocator* commandAllocator;
    ID3D12GraphicsCommandList1* commandList;
    DescriptorHeap SRV;
    DescriptorHeap RTV;
    IDXGISwapChain4* swapChain;
    ID3D12RootSignature* rootSignature;
    ID3D12Fence* fence;
    UINT64 fenceValue;
    HANDLE fenceEvent;
    UINT frameIndex;
    ID3D12Resource* renderTargets[2];
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

static BOOL CreateDescriptorHeap(DescriptorHeap* heap, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { 0 };
    heapDesc.Type = type;
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Flags = flags;
    if (FAILED(platform.device->lpVtbl->CreateDescriptorHeap(platform.device, &heapDesc, &IID_ID3D12DescriptorHeap, (LPVOID*)&heap->descriptorHeap)))
    {
        return FALSE;
    }

    heap->heapSize = platform.device->lpVtbl->GetDescriptorHandleIncrementSize(platform.device, type);
    return TRUE;
}

static BOOL InitializeRenderTarget(UINT index)
{
    if (FAILED(platform.swapChain->lpVtbl->GetBuffer(platform.swapChain, index, &IID_ID3D12Resource, (LPVOID*)&platform.renderTargets[index])))
    {
        printf("DIRECTX: Failed to retrieve buffer for render target index: %d!\n", index);
        return FALSE;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE offset = { 0 };
    platform.RTV.descriptorHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(platform.RTV.descriptorHeap, &offset);
    offset.ptr += platform.RTV.heapSize;
    platform.device->lpVtbl->CreateRenderTargetView(platform.device, platform.renderTargets[index], NULL, offset);

    return TRUE;
}

static void WaitForPreviousFrame()
{
    const UINT64 fenceValue = platform.fenceValue;
    if (FAILED(platform.commandQueue->lpVtbl->Signal(platform.commandQueue, platform.fence, fenceValue)))
    {
        return;
    }
    platform.fenceValue += 1;

    if (platform.fence->lpVtbl->GetCompletedValue(platform.fence) < fenceValue)
    {
        if (SUCCEEDED(platform.fence->lpVtbl->SetEventOnCompletion(platform.fence, fenceValue, platform.fenceEvent)))
        {
            WaitForSingleObject(platform.fenceEvent, INFINITE);
        }
    }
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

    ShowWindow(platform.handle, SW_SHOW);

    return 0;
}

void Windows_GetWindowSize(int* width, int* height)
{
    RECT bounds = { 0 };
    GetWindowRect(platform.handle, &bounds);

    if (width != NULL)
    {
        *width = bounds.right - bounds.left;
    }

    if (height != NULL)
    {
        *height = bounds.bottom - bounds.top;
    }
}

long long Windows_GetTime()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    return counter.QuadPart * (LONGLONG)1e9 / frequency.QuadPart;
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


    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = { 0 };
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    result = platform.device->lpVtbl->CreateCommandQueue(platform.device, &commandQueueDesc, &IID_ID3D12CommandQueue, (LPVOID*)&platform.commandQueue);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create command queue!\n");
        return -1;
    }

    result = platform.device->lpVtbl->CreateCommandAllocator(platform.device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (LPVOID*)&platform.commandAllocator);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create command allocator!\n");
        return -1;
    }

    result = platform.device->lpVtbl->CreateCommandList(platform.device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, platform.commandAllocator, NULL, &IID_ID3D12GraphicsCommandList1, (LPVOID*)&platform.commandList);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create command list!\n");
        return -1;
    }

    if (!CreateDescriptorHeap(&platform.SRV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE))
    {
        printf("DIRECTX: Failed to create SRV descriptor heap!\n");
        return -1;
    }

    D3D12_FEATURE_DATA_ROOT_SIGNATURE Feature = { D3D_ROOT_SIGNATURE_VERSION_1_1 };
    result = platform.device->lpVtbl->CheckFeatureSupport(platform.device, D3D12_FEATURE_ROOT_SIGNATURE, &Feature, sizeof(Feature));
    if (FAILED(result))
    {
        Feature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    D3D12_DESCRIPTOR_RANGE1 descriptorRanges[2];
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].RegisterSpace = 0;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descriptorRanges[1].NumDescriptors = 1;
    descriptorRanges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
    descriptorRanges[1].BaseShaderRegister = 0;
    descriptorRanges[1].RegisterSpace = 0;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER1 parameters[2];
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    parameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];

    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];

    D3D12_STATIC_SAMPLER_DESC sampler = { 0 };
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = { 0 };
    rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSignatureDesc.Desc_1_1.NumParameters = _countof(parameters);
    rootSignatureDesc.Desc_1_1.pParameters = parameters;
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
    rootSignatureDesc.Desc_1_1.pStaticSamplers = &sampler;
    rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* signature = NULL;
    ID3DBlob* error = NULL;
    result = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to serialize versioned root signature! Error: %s\n", (LPCSTR)error->lpVtbl->GetBufferPointer(error));
        return -1;
    }

    result = platform.device->lpVtbl->CreateRootSignature(platform.device, 0, signature->lpVtbl->GetBufferPointer(signature),
        signature->lpVtbl->GetBufferSize(signature), &IID_ID3D12RootSignature, (LPVOID*)&platform.rootSignature);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create root signature!\n");
        return -1;
    }

    result = platform.device->lpVtbl->CreateFence(platform.device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (LPVOID*)&platform.fence);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create fence!\n");
        return -1;
    }
    platform.fenceValue = 0;

    platform.fenceEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (platform.fenceEvent == NULL)
    {
        printf("DIRECTX: Failed to create fence event!\n");
        return -1;
    }

    if (!CreateDescriptorHeap(&platform.RTV, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_NONE))
    {
        printf("DIRECTX: Failed to create render target descriptors!\n");
        return -1;
    }

    IUnknown* unknownCommandQueue = NULL;
    result = platform.commandQueue->lpVtbl->QueryInterface(platform.commandQueue, &IID_IUnknown, (LPVOID*)&unknownCommandQueue);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to query for IUnknown command queue!\n");
        return -1;
    }

    int width, height;
    Windows_GetWindowSize(&width, &height);
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = (UINT)width;
    swapChainDesc.Height = (UINT)height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    IDXGISwapChain1* swapChain = NULL;
    result = platform.factory->lpVtbl->CreateSwapChainForHwnd(platform.factory, unknownCommandQueue, platform.handle, &swapChainDesc, NULL, NULL, &swapChain);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create swap chain!\n");
        return -1;
    }

    result = platform.factory->lpVtbl->MakeWindowAssociation(platform.factory, platform.handle, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to make window association!\n");
        return -1;
    }
    swapChain->lpVtbl->QueryInterface(swapChain, &IID_IDXGISwapChain4, (LPVOID*)&platform.swapChain);
    platform.frameIndex = platform.swapChain->lpVtbl->GetCurrentBackBufferIndex(platform.swapChain);

    for (int i = 0; i < 2; i++)
    {
        InitializeRenderTarget(i);
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
    DXRELEASE(platform.renderTargets[0]);
    DXRELEASE(platform.renderTargets[1]);
    DXRELEASE(platform.swapChain);
    DXRELEASE(platform.fence);
    DXRELEASE(platform.rootSignature);
    DXRELEASE(platform.RTV.descriptorHeap);
    DXRELEASE(platform.SRV.descriptorHeap);
    DXRELEASE(platform.commandList);
    DXRELEASE(platform.commandAllocator);
    DXRELEASE(platform.commandQueue);
    DXRELEASE(platform.adapter);
    DXRELEASE(platform.factory);
    DXRELEASE(platform.device);
}

void DirectX_Present()
{
    if (FAILED(platform.swapChain->lpVtbl->Present(platform.swapChain, 1, 0)))
    {
        printf("DIRECTX: Failed to present!\n");
    }

    WaitForPreviousFrame();
    platform.frameIndex = platform.swapChain->lpVtbl->GetCurrentBackBufferIndex(platform.swapChain);
}
