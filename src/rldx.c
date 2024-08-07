/**********************************************************************************************
*
*   rldx
*
*   DESCRIPTION:
*
*   ADDITIONAL NOTES:
*
*   CONFIGURATION:
*
*   DEPENDENCIES:
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2014-2024 Ramon Santamaria (@raysan5)
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

#include "rldx.h"
#include "platforms/rcore_desktop_windows_impl.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <stdio.h>

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------

#ifndef DIRECTX_INFOQUEUE
#define DIRECTX_INFOQUEUE
#endif

#ifndef DIRECTX_SHADER_DEBUG
// #define DIRECTX_SHADER_DEBUG
#endif

#define DXRELEASE(object) if (object != NULL) { object->lpVtbl->Release(object); object = NULL; }

#define NUM_DESCRIPTORS 100
#define NUM_TEXTURES NUM_DESCRIPTORS - 2

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------

typedef struct {
    unsigned char *data;
    size_t elementSize;
    size_t length;
    size_t capacity;
} Vector;

typedef struct {
    Vector pool;
    unsigned int index;
} ObjectPool;

typedef struct {
    unsigned int id;
    ID3D12Resource *data;
    ID3D12Resource *upload;
} DXTexture;

typedef struct {

typedef struct {
    ID3D12DescriptorHeap* descriptorHeap;
    UINT heapSize;
} DescriptorHeap;

typedef struct {
    ID3D12Device9 *device;
    IDXGIFactory7 *factory;
    IDXGIAdapter1 *adapter;
    ID3D12CommandQueue *commandQueue;
    ID3D12CommandAllocator *commandAllocator;
    ID3D12GraphicsCommandList1 *commandList;
    DescriptorHeap srv;
    DescriptorHeap rtv;
    IDXGISwapChain4 *swapChain;
    ID3D12RootSignature *rootSignature;
    ID3D12Fence *fence;
    UINT64 fenceValue;
    HANDLE fenceEvent;
    UINT frameIndex;
    ID3D12Resource *renderTargets[2];
    ID3D12Resource *constantBuffer;
    unsigned char *constantBufferPtr;
    ObjectPool textures;
    ID3D12PipelineState *defaultPipelineState;
#if defined(DIRECTX_INFOQUEUE)
    ID3D12InfoQueue* infoQueue;
#endif
} DriverData;

// Constant buffers need to be 256 byte aligned
typedef struct {
    Matrix mpv;
    char dummy[192];
} ConstantBuffer;

typedef struct {
    unsigned int defaultTextureId;
} DXState;

//----------------------------------------------------------------------------------
// Variables
//----------------------------------------------------------------------------------

static const int constantBufferIndex = NUM_DESCRIPTORS - 1;
static DriverData driver = { 0 };
static DXState dxState = { 0 };

//----------------------------------------------------------------------------------
// External functions
//----------------------------------------------------------------------------------

extern void *GetWindowHandle(void);

//----------------------------------------------------------------------------------
// Utility functions
//----------------------------------------------------------------------------------

static Vector VectorCreate(size_t elementSize)
{
    Vector result = { 0 };
    result.elementSize = elementSize;
    result.length = 0;
    result.capacity = 1;
    result.data = (unsigned char*)malloc(elementSize * result.capacity);
    return result;
}

static void VectorDestroy(Vector *vector)
{
    if (vector == NULL)
    {
        return;
    }

    if (vector->data != NULL)
    {
        free(vector->data);
    }

    vector->data = NULL;
    vector->elementSize = 0;
    vector->length = 0;
    vector->capacity = 0;
}

static void VectorResize(Vector *vector, size_t capacity)
{
    if (vector == NULL || vector->elementSize == 0)
    {
        return;
    }

    vector->capacity = capacity;
    vector->data = realloc(vector->data, vector->elementSize * capacity);
}

static void VectorPush(Vector *vector, void *element)
{
    if (vector == NULL || element == NULL || vector->elementSize == 0)
    {
        return;
    }

    if (vector->length == vector->capacity)
    {
        VectorResize(vector, vector->capacity * 2);
    }

    const size_t offset = vector->length * vector->elementSize;
    memcpy(vector->data + offset, element, vector->elementSize);
    vector->length++;
}

static bool VectorRemove(Vector *vector, size_t index)
{
    if (vector == NULL || vector->elementSize == 0 || index >= vector->length)
    {
        return false;
    }

    if (index == vector->length - 1)
    {
        vector->length--;
        return true;
    }

    unsigned char *start = vector->data + index * vector->elementSize;
    unsigned char *end = start + vector->elementSize;
    const size_t count = vector->length - (index + 1);
    const size_t size = count * vector->elementSize;
    memmove(start, end, size);
    vector->length--;

    return true;
}

static unsigned char *VectorGet(Vector *vector, size_t index)
{
    if (vector == NULL || vector->elementSize == 0 || index >= vector->length)
    {
        return NULL;
    }

    return &vector->data[vector->elementSize * index];
}

static bool EnumAdapter(UINT index, IDXGIFactory7* factory, IDXGIAdapter1** adapter)
{
    HRESULT result = factory->lpVtbl->EnumAdapterByGpuPreference(factory, index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter1, (LPVOID*)&(*adapter));
    return SUCCEEDED(result);
}

static bool IsValidAdapter(IDXGIAdapter1* adapter)
{
    DXGI_ADAPTER_DESC1 desc = { 0 };
    HRESULT result = adapter->lpVtbl->GetDesc1(adapter, &desc);

    if (FAILED(result))
    {
        printf("DIRECTX: Failed to retrieve description for adaptar!\n");
        return false;
    }

    if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
    {
        return false;
    }

    IUnknown* unknownAdapter = NULL;
    if (FAILED(adapter->lpVtbl->QueryInterface(adapter, &IID_IUnknown, (LPVOID*)&unknownAdapter)))
    {
        return false;
    }

    result = D3D12CreateDevice(unknownAdapter, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device9, NULL);
    if (FAILED(result))
    {
        return false;
    }

    return true;
}

static bool CreateDescriptorHeap(DescriptorHeap* heap, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { 0 };
    heapDesc.Type = type;
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Flags = flags;
    if (FAILED(driver.device->lpVtbl->CreateDescriptorHeap(driver.device, &heapDesc, &IID_ID3D12DescriptorHeap, (LPVOID*)&heap->descriptorHeap)))
    {
        return false;
    }

    heap->heapSize = driver.device->lpVtbl->GetDescriptorHandleIncrementSize(driver.device, type);
    return true;
}

static bool InitializeDevice()
{
    UINT factoryFlags = 0;
#if defined(DIRECTX_INFOQUEUE)
    ID3D12Debug *debug = NULL;
    if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (LPVOID*)&debug)))
    {
        debug->lpVtbl->EnableDebugLayer(debug);
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    IDXGIFactory7* factory = NULL;
    HRESULT result = CreateDXGIFactory2(factoryFlags, &IID_IDXGIFactory7, (LPVOID*)&factory);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create DXGI Factory!\n");
        return false;
    }

    IDXGIFactory7* queryFactory = NULL;
    result = factory->lpVtbl->QueryInterface(factory, &IID_IDXGIFactory7, (LPVOID*)&queryFactory);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to query factory interface!\n");
        return false;
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
    driver.factory = factory;
    driver.adapter = adapter;

    IUnknown* unknownAdapter = NULL;
    if (FAILED(adapter->lpVtbl->QueryInterface(adapter, &IID_IUnknown, (LPVOID*)&unknownAdapter)))
    {
        printf("DIRECTX: Failed to query IUnknown for IDXGIAdapter!\n");
        return false;
    }

    result = D3D12CreateDevice(unknownAdapter, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device9, (LPVOID*)&driver.device);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create device!\n");
        return false;
    }

    return true;
}

static bool InitializeCommands()
{
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = { 0 };
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HRESULT result = driver.device->lpVtbl->CreateCommandQueue(driver.device, &commandQueueDesc, &IID_ID3D12CommandQueue, (LPVOID*)&driver.commandQueue);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create command queue!\n");
        return false;
    }

    result = driver.device->lpVtbl->CreateCommandAllocator(driver.device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (LPVOID*)&driver.commandAllocator);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create command allocator!\n");
        return false;
    }

    result = driver.device->lpVtbl->CreateCommandList(driver.device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, driver.commandAllocator, NULL, &IID_ID3D12GraphicsCommandList1, (LPVOID*)&driver.commandList);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create command list!\n");
        return false;
    }

    return true;
}

static bool InitializeRootSignature()
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE feature = { D3D_ROOT_SIGNATURE_VERSION_1_1 };
    HRESULT result = driver.device->lpVtbl->CheckFeatureSupport(driver.device, D3D12_FEATURE_ROOT_SIGNATURE, &feature, sizeof(feature));
    if (FAILED(result))
    {
        feature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
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
        return false;
    }

    result = driver.device->lpVtbl->CreateRootSignature(driver.device, 0, signature->lpVtbl->GetBufferPointer(signature),
        signature->lpVtbl->GetBufferSize(signature), &IID_ID3D12RootSignature, (LPVOID*)&driver.rootSignature);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create root signature!\n");
        return false;
    }

    return true;
}

static bool InitializeFence()
{
    HRESULT result = driver.device->lpVtbl->CreateFence(driver.device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (LPVOID*)&driver.fence);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create fence!\n");
        return false;
    }
    driver.fenceValue = 0;

    driver.fenceEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (driver.fenceEvent == NULL)
    {
        printf("DIRECTX: Failed to create fence event!\n");
        return false;
    }

    return true;
}

static D3D12_CPU_DESCRIPTOR_HANDLE CPUOffset(DescriptorHeap *heap, UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE result = { 0 };
    heap->descriptorHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap->descriptorHeap, &result);
    result.ptr += index * heap->heapSize;
    return result;
}

static D3D12_GPU_DESCRIPTOR_HANDLE GPUOffset(DescriptorHeap *heap, UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE result = { 0 };
    heap->descriptorHeap->lpVtbl->GetGPUDescriptorHandleForHeapStart(heap->descriptorHeap, &result);
    result.ptr += index * heap->heapSize;
    return result;
}

static bool InitializeRenderTarget(UINT index)
{
    if (FAILED(driver.swapChain->lpVtbl->GetBuffer(driver.swapChain, index, &IID_ID3D12Resource, (LPVOID*)&driver.renderTargets[index])))
    {
        printf("DIRECTX: Failed to retrieve buffer for render target index: %d!\n", index);
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE offset = CPUOffset(&driver.rtv, index);
    driver.device->lpVtbl->CreateRenderTargetView(driver.device, driver.renderTargets[index], NULL, offset);

    return TRUE;
}

static bool InitializeSwapChain(UINT width, UINT height)
{
    if (!CreateDescriptorHeap(&driver.rtv, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_NONE))
    {
        printf("DIRECTX: Failed to create render target descriptors!\n");
        return false;
    }

    IUnknown* unknownCommandQueue = NULL;
    HRESULT result = driver.commandQueue->lpVtbl->QueryInterface(driver.commandQueue, &IID_IUnknown, (LPVOID*)&unknownCommandQueue);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to query for IUnknown command queue!\n");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    HANDLE handle = (HANDLE)GetWindowHandle();
    IDXGISwapChain1* swapChain = NULL;
    result = driver.factory->lpVtbl->CreateSwapChainForHwnd(driver.factory, unknownCommandQueue, handle, &swapChainDesc, NULL, NULL, &swapChain);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create swap chain!\n");
        return false;
    }

    result = driver.factory->lpVtbl->MakeWindowAssociation(driver.factory, handle, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to make window association!\n");
        return false;
    }
    swapChain->lpVtbl->QueryInterface(swapChain, &IID_IDXGISwapChain4, (LPVOID*)&driver.swapChain);
    driver.frameIndex = driver.swapChain->lpVtbl->GetCurrentBackBufferIndex(driver.swapChain);

    for (int i = 0; i < 2; i++)
    {
        InitializeRenderTarget(i);
    }

    return true;
}

static bool InitializeConstantBuffer()
{
    D3D12_HEAP_PROPERTIES properties = { 0 };
    properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC description = { 0 };
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Alignment = 0;
    description.Width = sizeof(ConstantBuffer);
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &properties,
        D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (LPVOID*)&driver.constantBuffer);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create constant buffer resource!\n");
        return false;
    }

    D3D12_CONSTANT_BUFFER_VIEW_DESC view = { 0 };
    view.BufferLocation = driver.constantBuffer->lpVtbl->GetGPUVirtualAddress(driver.constantBuffer);
    view.SizeInBytes = (UINT)description.Width;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = CPUOffset(&driver.srv, constantBufferIndex);
    driver.device->lpVtbl->CreateConstantBufferView(driver.device, &view, handle);

    D3D12_RANGE range = { 0 };
    result = driver.constantBuffer->lpVtbl->Map(driver.constantBuffer, 0, &range, (LPVOID*)&driver.constantBufferPtr);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to map constant buffer memory!\n");
        return false;
    }

    return true;
}

#if defined(DIRECTX_INFOQUEUE)
static bool InitializeInfoQueue()
{
    if (FAILED(driver.device->lpVtbl->QueryInterface(driver.device, &IID_ID3D12InfoQueue, (LPVOID*)&driver.infoQueue)))
    {
        printf("DIRECTX: Failed to initialize info queue!\n");
        return false;
    }

    return true;
}

static void PollInfoQueue()
{
    if (driver.infoQueue == NULL)
    {
        return;
    }

    const UINT64 count = driver.infoQueue->lpVtbl->GetNumStoredMessagesAllowedByRetrievalFilter(driver.infoQueue);
    for (UINT64 i = 0; i < count; i++)
    {
        SIZE_T length = 0;
        if (FAILED(driver.infoQueue->lpVtbl->GetMessage(driver.infoQueue, i, NULL, &length)))
        {
            continue;
        }

        if (length == 0)
        {
            continue;
        }

        D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(length);
        if (SUCCEEDED(driver.infoQueue->lpVtbl->GetMessage(driver.infoQueue, i, message, &length)))
        {
            printf("DIRECTX: %s\n", message->pDescription);
        }
        free(message);
    }

    driver.infoQueue->lpVtbl->ClearStoredMessages(driver.infoQueue);
}
#endif

static void WaitForPreviousFrame()
{
    const UINT64 fenceValue = driver.fenceValue;
    if (FAILED(driver.commandQueue->lpVtbl->Signal(driver.commandQueue, driver.fence, fenceValue)))
    {
        return;
    }
    driver.fenceValue += 1;

    if (driver.fence->lpVtbl->GetCompletedValue(driver.fence) < fenceValue)
    {
        if (SUCCEEDED(driver.fence->lpVtbl->SetEventOnCompletion(driver.fence, fenceValue, driver.fenceEvent)))
        {
            WaitForSingleObject(driver.fenceEvent, INFINITE);
        }
    }
}

static bool ExecuteCommands()
{
    if (FAILED(driver.commandList->lpVtbl->Close(driver.commandList)))
    {
        return false;
    }

    ID3D12CommandList* commandList = NULL;
    driver.commandList->lpVtbl->QueryInterface(driver.commandList, &IID_ID3D12CommandList, (LPVOID*)&commandList);

    ID3D12CommandList* commands[] = { commandList };
    driver.commandQueue->lpVtbl->ExecuteCommandLists(driver.commandQueue, _countof(commands), commands);

    return true;
}

static bool ResetCommands()
{
    if (FAILED(driver.commandAllocator->lpVtbl->Reset(driver.commandAllocator)))
    {
        return false;
    }

    if (FAILED(driver.commandList->lpVtbl->Reset(driver.commandList, driver.commandAllocator, NULL)))
    {
        return false;
    }

    return true;
}

static void UpdateRenderTarget()
{
    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.pResource = driver.renderTargets[driver.frameIndex];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    driver.commandList->lpVtbl->ResourceBarrier(driver.commandList, 1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CPUOffset(&driver.rtv, driver.frameIndex);
    driver.commandList->lpVtbl->OMSetRenderTargets(driver.commandList, 1, &rtvHandle, FALSE, NULL);
}

static bool InitializeDefaultShader()
{
    const char *vertexShaderCode =
    "struct PSInput                         \n"
    "{                                      \n"
    "   float4 position : SV_POSITION;      \n"
    "   float2 uv : TEXCOORD;               \n"
    "};                                     \n"

    "cbuffer ConstantBuffer : register(b0)  \n"
    "{                                      \n"
    "   float4x4 mvp;                       \n"
    "   int pad[6];                         \n"
    "}                                      \n"

    "PSInput Main(float4 position : POSITION, float2 uv : TEXCOORD)     \n"
    "{                                                                  \n"
    "   PSInput result;                                                 \n"
    "   result.position = mul(mvp, float4(position.xyz, 1.0));          \n"
    "   result.uv = uv;                                                 \n"
    "   return result;                                                  \n"
    "}";

    const char *fragmentShaderCode =
    "struct PSInput                         \n"
    "{                                      \n"
    "   float4 position : SV_POSITION;      \n"
    "   float2 uv : TEXCOORD;               \n"
    "};                                     \n"
    "Texture2D g_Texture : register(t0);    \n"
    "SamplerState g_Sampler : register(s0); \n"
    "float4 Main(PSInput input) : SV_TARGET \n"
    "{                                      \n"
    "   return g_Texture.Sample(g_Sampler, input.uv); \n"
    "}";

    const size_t vertexShaderCodeLen = strlen(vertexShaderCode);
    const size_t fragmentShaderCodeLen = strlen(fragmentShaderCode);
    UINT compileFlags = 0;

#if defined(DIRECTX_SHADER_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob *vertexShaderBlob = NULL;
    ID3DBlob *fragmentShaderBlob = NULL;
    ID3DBlob *errors = NULL;

    HRESULT result = D3DCompile(vertexShaderCode, vertexShaderCodeLen, "defaultVS", NULL, NULL, "Main", "vs_5_0", compileFlags, 0, &vertexShaderBlob, &errors);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to compile default vertex shader! Error: %s\n", (LPCSTR)errors->lpVtbl->GetBufferPointer(errors));
        return false;
    }

    result = D3DCompile(fragmentShaderCode, fragmentShaderCodeLen, "defaultFS", NULL, NULL, "Main", "ps_5_0", compileFlags, 0, &fragmentShaderBlob, &errors);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to compile default fragment shader! Error: %s\n", (LPCSTR)errors->lpVtbl->GetBufferPointer(errors));
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC elements[2] = { {0}, {0} };
    elements[0].SemanticName = "POSITION";
    elements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    elements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    elements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;

    elements[1].SemanticName = "TEXCOORD";
    elements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    elements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    elements[1].Format = DXGI_FORMAT_R32G32_FLOAT;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc = { 0 };
    graphicsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    graphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    graphicsDesc.RasterizerState.FrontCounterClockwise = TRUE;
    graphicsDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    graphicsDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    graphicsDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    graphicsDesc.RasterizerState.DepthClipEnable = TRUE;
    graphicsDesc.RasterizerState.MultisampleEnable = FALSE;
    graphicsDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    graphicsDesc.RasterizerState.ForcedSampleCount = 0;
    graphicsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    graphicsDesc.BlendState.AlphaToCoverageEnable = FALSE;
    graphicsDesc.BlendState.IndependentBlendEnable = FALSE;
    for (UINT I = 0; I < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++I)
    {
        graphicsDesc.BlendState.RenderTarget[I].BlendEnable = TRUE;
        graphicsDesc.BlendState.RenderTarget[I].LogicOpEnable = FALSE;
        graphicsDesc.BlendState.RenderTarget[I].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        graphicsDesc.BlendState.RenderTarget[I].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        graphicsDesc.BlendState.RenderTarget[I].BlendOp = D3D12_BLEND_OP_ADD;
        graphicsDesc.BlendState.RenderTarget[I].SrcBlendAlpha = D3D12_BLEND_ONE;
        graphicsDesc.BlendState.RenderTarget[I].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        graphicsDesc.BlendState.RenderTarget[I].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        graphicsDesc.BlendState.RenderTarget[I].LogicOp = D3D12_LOGIC_OP_NOOP;
        graphicsDesc.BlendState.RenderTarget[I].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    graphicsDesc.SampleMask = UINT_MAX;
    graphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsDesc.NumRenderTargets = 1;
    graphicsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsDesc.SampleDesc.Count = 1;

    graphicsDesc.InputLayout.pInputElementDescs = elements;
    graphicsDesc.InputLayout.NumElements = _countof(elements);
    graphicsDesc.pRootSignature = driver.rootSignature;
    graphicsDesc.VS.pShaderBytecode = vertexShaderBlob->lpVtbl->GetBufferPointer(vertexShaderBlob);
    graphicsDesc.VS.BytecodeLength = vertexShaderBlob->lpVtbl->GetBufferSize(vertexShaderBlob);
    graphicsDesc.PS.pShaderBytecode = fragmentShaderBlob->lpVtbl->GetBufferPointer(fragmentShaderBlob);
    graphicsDesc.PS.BytecodeLength = fragmentShaderBlob->lpVtbl->GetBufferSize(fragmentShaderBlob);

    graphicsDesc.DepthStencilState.DepthEnable = TRUE;
    graphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    graphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    graphicsDesc.DepthStencilState.StencilEnable = TRUE;
    graphicsDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    graphicsDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    graphicsDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    graphicsDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    graphicsDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    graphicsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    graphicsDesc.DepthStencilState.BackFace = graphicsDesc.DepthStencilState.FrontFace;
    graphicsDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    bool success = true;
    result = driver.device->lpVtbl->CreateGraphicsPipelineState(driver.device, &graphicsDesc, &IID_ID3D12PipelineState, (LPVOID*)&driver.defaultPipelineState);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create default graphics pipeline state!\n");
        success = false;
    }

    DXRELEASE(vertexShaderBlob);
    DXRELEASE(fragmentShaderBlob);

    return success;
}

static void BindDefaultPipeline()
{
    driver.commandList->lpVtbl->SetPipelineState(driver.commandList, driver.defaultPipelineState);
}

//----------------------------------------------------------------------------------
// API
//----------------------------------------------------------------------------------

void rlMatrixMode(int mode) {}
void rlPushMatrix(void) {}
void rlPopMatrix(void) {}
void rlLoadIdentity(void) {}
void rlTranslatef(float x, float y, float z) {}
void rlRotatef(float angle, float x, float y, float z) {}
void rlScalef(float x, float y, float z) {}
void rlMultMatrixf(const float *matf) {}
void rlFrustum(double left, double right, double bottom, double top, double znear, double zfar) {}
void rlOrtho(double left, double right, double bottom, double top, double znear, double zfar) {}
void rlViewport(int x, int y, int width, int height) {}
void rlSetClipPlanes(double nearPlane, double farPlane) {}
double rlGetCullDistanceNear(void) { return 0.0; }
double rlGetCullDistanceFar(void) { return 0.0; }

//------------------------------------------------------------------------------------
// Functions Declaration - Vertex level operations
//------------------------------------------------------------------------------------
void rlBegin(int mode) {}
void rlEnd(void) {}
void rlVertex2i(int x, int y) {}
void rlVertex2f(float x, float y) {}
void rlVertex3f(float x, float y, float z) {}
void rlTexCoord2f(float x, float y) {}
void rlNormal3f(float x, float y, float z) {}
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {}
void rlColor3f(float x, float y, float z) {}
void rlColor4f(float x, float y, float z, float w) {}

//------------------------------------------------------------------------------------
// Functions Declaration - OpenGL style functions (common to 1.1, 3.3+, ES2)
// NOTE: This functions are used to completely abstract raylib code from OpenGL layer,
// some of them are direct wrappers over OpenGL calls, some others are custom
//------------------------------------------------------------------------------------

// Vertex buffers state
bool rlEnableVertexArray(unsigned int vaoId) { return false; }
void rlDisableVertexArray(void) {}
void rlEnableVertexBuffer(unsigned int id) {}
void rlDisableVertexBuffer(void) {}
void rlEnableVertexBufferElement(unsigned int id) {}
void rlDisableVertexBufferElement(void) {}
void rlEnableVertexAttribute(unsigned int index) {}
void rlDisableVertexAttribute(unsigned int index) {}

// Textures state
void rlActiveTextureSlot(int slot) {}
void rlEnableTexture(unsigned int id) {}
void rlDisableTexture(void) {}
void rlEnableTextureCubemap(unsigned int id) {}
void rlDisableTextureCubemap(void) {}
void rlTextureParameters(unsigned int id, int param, int value) {}
void rlCubemapParameters(unsigned int id, int param, int value) {}

// Shader state
void rlEnableShader(unsigned int id) {}
void rlDisableShader(void) {}

// Framebuffer state
void rlEnableFramebuffer(unsigned int id) {}
void rlDisableFramebuffer(void) {}
unsigned int rlGetActiveFramebuffer(void) { return 0; }
void rlActiveDrawBuffers(int count) {}
void rlBlitFramebuffer(int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight, int bufferMask) {}
void rlBindFramebuffer(unsigned int target, unsigned int framebuffer) {}

// General render state
void rlEnableColorBlend(void) {}
void rlDisableColorBlend(void) {}
void rlEnableDepthTest(void) {}
void rlDisableDepthTest(void) {}
void rlEnableDepthMask(void) {}
void rlDisableDepthMask(void) {}
void rlEnableBackfaceCulling(void) {}
void rlDisableBackfaceCulling(void) {}
void rlColorMask(bool r, bool g, bool b, bool a) {}
void rlSetCullFace(int mode) {}
void rlEnableScissorTest(void) {}
void rlDisableScissorTest(void) {}
void rlScissor(int x, int y, int width, int height) {}
void rlEnableWireMode(void) {}
void rlEnablePointMode(void) {}
void rlDisableWireMode(void) {}
void rlSetLineWidth(float width) {}
float rlGetLineWidth(void) { return 0.0f; }
void rlEnableSmoothLines(void) {}
void rlDisableSmoothLines(void) {}
void rlEnableStereoRender(void) {}
void rlDisableStereoRender(void) {}
bool rlIsStereoRenderEnabled(void) { return false; }

void rlClearColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    float color[4] = { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f };
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = CPUOffset(&driver.rtv, driver.frameIndex);
    driver.commandList->lpVtbl->ClearRenderTargetView(driver.commandList, rtv, color, 0, NULL);
}

void rlClearScreenBuffers(void) {}
void rlCheckErrors(void) {}
void rlSetBlendMode(int mode) {}
void rlSetBlendFactors(int glSrcFactor, int glDstFactor, int glEquation) {}
void rlSetBlendFactorsSeparate(int glSrcRGB, int glDstRGB, int glSrcAlpha, int glDstAlpha, int glEqRGB, int glEqAlpha) {}


void rlglInit(int width, int height)
{
    if (!InitializeDevice())
    {
        return;
    }

    if (!InitializeCommands())
    {
        return;
    }

    if (!CreateDescriptorHeap(&driver.srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_DESCRIPTORS, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE))
    {
        printf("DIRECTX: Failed to create SRV descriptor heap!\n");
        return;
    }

    if (!InitializeRootSignature())
    {
        return;
    }

#if defined(DIRECTX_INFOQUEUE)
    InitializeInfoQueue();
#endif

    if (!InitializeFence())
    {
        return;
    }

    if (!InitializeSwapChain(width, height))
    {
        return;
    }

    if (!InitializeConstantBuffer())
    {
        return;
    }

    if (!InitializeDefaultShader())
    {
        return;
    }

    BindDefaultPipeline();

    DXGI_ADAPTER_DESC1 desc = { 0 };
    if (FAILED(driver.adapter->lpVtbl->GetDesc1(driver.adapter, &desc)))
    {
        printf("DIRECTX: Failed to retrieve adapter description!\n");
        return;
    }

    driver.textures.pool = VectorCreate(sizeof(DXTexture));
    driver.textures.index = 1;

    printf("DIRECTX: Initialized DirectX!\n");

    char* driverName = Windows_ToMultiByte(desc.Description);
    printf("DIRECTX: Driver is %s.\n", driverName);
    free(driverName);

    // Init default white texture
    unsigned char pixels[4] = { 255, 255, 255, 255 };   // 1 pixel RGBA (4 bytes)
    dxState.defaultTextureId = rlLoadTexture(pixels, 1, 1, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);

    UpdateRenderTarget();
}

void rlglClose(void)
{
    for (size_t i = 0; i < driver.textures.pool.length; i++)
    {
        DXTexture *texture = (DXTexture*)VectorGet(&driver.textures.pool, i);
        DXRELEASE(texture->data);
        DXRELEASE(texture->upload);
    }
    VectorDestroy(&driver.textures.pool);

    DXRELEASE(driver.constantBuffer);
    DXRELEASE(driver.renderTargets[0]);
    DXRELEASE(driver.renderTargets[1]);
    DXRELEASE(driver.swapChain);
    DXRELEASE(driver.fence);
    DXRELEASE(driver.rootSignature);
    DXRELEASE(driver.rtv.descriptorHeap);
    DXRELEASE(driver.srv.descriptorHeap);
    DXRELEASE(driver.commandList);
    DXRELEASE(driver.commandAllocator);
    DXRELEASE(driver.commandQueue);
    DXRELEASE(driver.adapter);
    DXRELEASE(driver.factory);
    DXRELEASE(driver.device);
}

void rlLoadExtensions(void *loader) {}
int rlGetVersion(void) { return 0; }
void rlSetFramebufferWidth(int width) {}
int rlGetFramebufferWidth(void) { return 0; }
void rlSetFramebufferHeight(int height) {}
int rlGetFramebufferHeight(void) { return 0; }

unsigned int rlGetTextureIdDefault(void) { return 0; }
unsigned int rlGetShaderIdDefault(void) { return 0; }
int *rlGetShaderLocsDefault(void) { return 0; }

// Render batch management
rlRenderBatch rlLoadRenderBatch(int numBuffers, int bufferElements) { rlRenderBatch result = {0}; return result; }
void rlUnloadRenderBatch(rlRenderBatch batch) {}
void rlDrawRenderBatch(rlRenderBatch *batch) {}
void rlSetRenderBatchActive(rlRenderBatch *batch) {}

void rlDrawRenderBatchActive(void)
{
    // TODO: Enable when graphics pipeline states are implemented.
    // D3D12_GPU_DESCRIPTOR_HANDLE constantBufferOffset = GPUOffset(&driver.srv, constantBufferIndex);
    // driver.commandList->lpVtbl->SetGraphicsRootDescriptorTable(driver.commandList, 0, constantBufferOffset);

    driver.commandList->lpVtbl->IASetPrimitiveTopology(driver.commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.pResource = driver.renderTargets[driver.frameIndex];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    driver.commandList->lpVtbl->ResourceBarrier(driver.commandList, 1, &barrier);

    ExecuteCommands();
    WaitForPreviousFrame();
}

bool rlCheckRenderBatchLimit(int vCount) { return false; }

void rlSetTexture(unsigned int id) {}

// Vertex buffers management
unsigned int rlLoadVertexArray(void) { return 0; }
unsigned int rlLoadVertexBuffer(const void *buffer, int size, bool dynamic) { return 0; }
unsigned int rlLoadVertexBufferElement(const void *buffer, int size, bool dynamic) { return 0; }
void rlUpdateVertexBuffer(unsigned int bufferId, const void *data, int dataSize, int offset) {}
void rlUpdateVertexBufferElements(unsigned int id, const void *data, int dataSize, int offset) {}
void rlUnloadVertexArray(unsigned int vaoId) {}
void rlUnloadVertexBuffer(unsigned int vboId) {}
void rlSetVertexAttribute(unsigned int index, int compSize, int type, bool normalized, int stride, int offset) {}
void rlSetVertexAttributeDivisor(unsigned int index, int divisor) {}
void rlSetVertexAttributeDefault(int locIndex, const void *value, int attribType, int count) {}
void rlDrawVertexArray(int offset, int count) {}
void rlDrawVertexArrayElements(int offset, int count, const void *buffer) {}
void rlDrawVertexArrayInstanced(int offset, int count, int instances) {}
void rlDrawVertexArrayElementsInstanced(int offset, int count, const void *buffer, int instances) {}

// Textures management
unsigned int rlLoadTexture(const void *data, int width, int height, int format, int mipmapCount)
{
    DXTexture texture = { 0 };

    D3D12_HEAP_PROPERTIES heap = { 0 };
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC description = { 0 };
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Alignment = 0;
    description.Width = width;
    description.Height = height;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    description.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (LPVOID*)&texture.data);
    if (FAILED(result))
    {
        printf("DIRECTX: Failed to create texture resource!\n");
        return 0;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = { 0 };
    UINT numRows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;
    driver.device->lpVtbl->GetCopyableFootprints(driver.device, &description, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &totalBytes);

    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Width = totalBytes;
    description.Height = 1;
    
    result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (LPVOID*)&texture.upload);
    if (FAILED(result))
    {
        DXRELEASE(texture.data);
        printf("DIRECTX: Failed to create texture upload resource!\n");
        return 0;
    }

    unsigned char *uploadBuffer = NULL;
    result = texture.upload->lpVtbl->Map(texture.upload, 0, NULL, (LPVOID*)&uploadBuffer);
    if (FAILED(result))
    {
        DXRELEASE(texture.data);
        DXRELEASE(texture.upload);
        printf("DIRECTX: Failed to map upload resource memory!\n");
        return 0;
    }

    D3D12_MEMCPY_DEST memcpyDest = { 0 };
    memcpyDest.pData = uploadBuffer + layouts.Offset;
    memcpyDest.RowPitch = layouts.Footprint.RowPitch;
    memcpyDest.SlicePitch = (UINT64)layouts.Footprint.RowPitch * (UINT64)numRows;

    D3D12_SUBRESOURCE_DATA textureData = { 0 };
    textureData.pData = data;
    textureData.RowPitch = format == RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA ? width * 2 : width * 4;
    textureData.SlicePitch = textureData.RowPitch * height;

    for (unsigned int slice = 0; slice < layouts.Footprint.Depth; slice++)
    {
        unsigned char *dest = (unsigned char*)memcpyDest.pData + memcpyDest.SlicePitch * slice;
        const unsigned char *src = (unsigned char*)textureData.pData + textureData.SlicePitch * (UINT64)slice;

        for (unsigned int row = 0; row < numRows; row++)
        {
            memcpy(dest + memcpyDest.RowPitch * row, src + textureData.RowPitch * (UINT64)row, rowSizeInBytes);
        }
    }

    texture.upload->lpVtbl->Unmap(texture.upload, 0, NULL);

    D3D12_TEXTURE_COPY_LOCATION copyDest = { 0 };
    copyDest.pResource = texture.data;
    copyDest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copyDest.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION copySrc = { 0 };
    copySrc.pResource = texture.upload;
    copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    copySrc.PlacedFootprint = layouts;

    driver.commandList->lpVtbl->CopyTextureRegion(driver.commandList, &copyDest, 0, 0, 0, &copySrc, NULL);

    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.pResource = texture.data;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    driver.commandList->lpVtbl->ResourceBarrier(driver.commandList, 1, &barrier);

    D3D12_SHADER_RESOURCE_VIEW_DESC shaderViewDesc = { 0 };
    shaderViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    shaderViewDesc.Format = description.Format;
    shaderViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    shaderViewDesc.Texture2D.MipLevels = 1;

    const UINT index = (UINT)driver.textures.pool.length;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuOffset = CPUOffset(&driver.srv, index);
    driver.device->lpVtbl->CreateShaderResourceView(driver.device, texture.data, &shaderViewDesc, cpuOffset);
    texture.id = driver.textures.index++;

    VectorPush(&driver.textures.pool, &texture);

    return texture.id;
}

unsigned int rlLoadTextureDepth(int width, int height, bool useRenderBuffer) { return 0; }
unsigned int rlLoadTextureCubemap(const void *data, int size, int format) { return 0; }
void rlUpdateTexture(unsigned int id, int offsetX, int offsetY, int width, int height, int format, const void *data) {}
void rlGetGlTextureFormats(int format, unsigned int *glInternalFormat, unsigned int *glFormat, unsigned int *glType) {}
const char *rlGetPixelFormatName(unsigned int format) { return ""; }
void rlUnloadTexture(unsigned int id) {}
void rlGenTextureMipmaps(unsigned int id, int width, int height, int format, int *mipmaps) {}
void *rlReadTexturePixels(unsigned int id, int width, int height, int format) { return NULL; }
unsigned char *rlReadScreenPixels(int width, int height) { return ""; }

// Framebuffer management (fbo)
unsigned int rlLoadFramebuffer(void) { return 0; }
void rlFramebufferAttach(unsigned int fboId, unsigned int texId, int attachType, int texType, int mipLevel) {}
bool rlFramebufferComplete(unsigned int id) { return false; }
void rlUnloadFramebuffer(unsigned int id) {}

// Shaders management
unsigned int rlLoadShaderCode(const char *vsCode, const char *fsCode) { return 0; }
unsigned int rlCompileShader(const char *shaderCode, int type) { return 0; }
unsigned int rlLoadShaderProgram(unsigned int vShaderId, unsigned int fShaderId) { return 0; }
void rlUnloadShaderProgram(unsigned int id) {}
int rlGetLocationUniform(unsigned int shaderId, const char *uniformName) { return 0; }
int rlGetLocationAttrib(unsigned int shaderId, const char *attribName) { return 0; }
void rlSetUniform(int locIndex, const void *value, int uniformType, int count) {}
void rlSetUniformMatrix(int locIndex, Matrix mat) {}
void rlSetUniformSampler(int locIndex, unsigned int textureId) {}
void rlSetShader(unsigned int id, int *locs) {}

// Compute shader management
unsigned int rlLoadComputeShaderProgram(unsigned int shaderId) { return 0; }
void rlComputeShaderDispatch(unsigned int groupX, unsigned int groupY, unsigned int groupZ) {}

// Shader buffer storage object management (ssbo)
unsigned int rlLoadShaderBuffer(unsigned int size, const void *data, int usageHint) { return 0; }
void rlUnloadShaderBuffer(unsigned int ssboId) {}
void rlUpdateShaderBuffer(unsigned int id, const void *data, unsigned int dataSize, unsigned int offset) {}
void rlBindShaderBuffer(unsigned int id, unsigned int index) {}
void rlReadShaderBuffer(unsigned int id, void *dest, unsigned int count, unsigned int offset) {}
void rlCopyShaderBuffer(unsigned int destId, unsigned int srcId, unsigned int destOffset, unsigned int srcOffset, unsigned int count) {}
unsigned int rlGetShaderBufferSize(unsigned int id) { return 0; }

// Buffer management
void rlBindImageTexture(unsigned int id, unsigned int index, int format, bool readonly) {}

// Matrix state management
Matrix rlGetMatrixModelview(void) { Matrix result = { 0 }; return result; }
Matrix rlGetMatrixProjection(void) { Matrix result = { 0 }; return result; }
Matrix rlGetMatrixTransform(void) { Matrix result = { 0 }; return result; }
Matrix rlGetMatrixProjectionStereo(int eye) { Matrix result = { 0 }; return result; }
Matrix rlGetMatrixViewOffsetStereo(int eye) { Matrix result = { 0 }; return result; }
void rlSetMatrixProjection(Matrix proj) {}
void rlSetMatrixModelview(Matrix view) {}
void rlSetMatrixProjectionStereo(Matrix right, Matrix left) {}
void rlSetMatrixViewOffsetStereo(Matrix right, Matrix left) {}

// Quick and dirty cube/quad buffers load->draw->unload
void rlLoadDrawCube(void) {}
void rlLoadDrawQuad(void) {}

void rlPresent()
{
    if (FAILED(driver.swapChain->lpVtbl->Present(driver.swapChain, 1, 0)))
    {
        printf("DIRECTX: Failed to present!\n");
    }

    ResetCommands();

    driver.frameIndex = driver.swapChain->lpVtbl->GetCurrentBackBufferIndex(driver.swapChain);

#if defined(DIRECTX_INFOQUEUE)
    PollInfoQueue();
#endif

    // Prepare render target for next frame
    UpdateRenderTarget();
    BindDefaultPipeline();
}
