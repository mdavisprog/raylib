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
#include <math.h>
#include <stdio.h>

//----------------------------------------------------------------------------------
// External functions
//----------------------------------------------------------------------------------

extern void *GetWindowHandle(void);
extern void TraceLog(int, const char*, ...);

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

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

#ifndef DEG2RAD
    #define DEG2RAD (PI/180.0f)
#endif

#ifndef RAD2DEG
    #define RAD2DEG (180.0f/PI)
#endif

#ifdef TRACELOG
    #undef TRACELOG
#endif

#define TRACELOG(level, ...) TraceLog(level, __VA_ARGS__)
#define DXTRACELOG(level, msg, ...) TRACELOG(level, "DIRECTX: " ##msg, __VA_ARGS__);

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
    unsigned int offset;
    ID3D12Resource *data;
    ID3D12Resource *upload;
    int width;
    int height;
} DXTexture;

typedef struct {
    unsigned int id;
    unsigned int type;
    ID3DBlob *data;
} DXShader;

typedef struct {
    unsigned int id;
    ID3D12PipelineState *state;
} DXPipeline;

typedef struct {
    ID3D12Resource *buffer;
    ID3D12Resource *uploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW view;
} DXVertexBuffer;

typedef struct {
    unsigned int id;
    DXVertexBuffer vertex;
    DXVertexBuffer texcoord;
    DXVertexBuffer normal;
    DXVertexBuffer color;
    ID3D12Resource *index;
    D3D12_INDEX_BUFFER_VIEW indexView;
} DXRenderBuffer;

typedef struct {
    ID3D12DescriptorHeap* descriptorHeap;
    UINT heapSize;
} DescriptorHeap;

typedef struct {
    DescriptorHeap descriptor;
    ID3D12Resource *resource;
} DepthStencil;

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
    ObjectPool renderBuffers;
    ObjectPool pipelines;
    ObjectPool shaders;
    DepthStencil depthStencil;
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
    Matrix modelView;
    Matrix projection;
    Matrix transform;
    Matrix stack[RL_MAX_MATRIX_STACK_SIZE];
    Matrix *currentMatrix;
    int stackCounter;
    int currentMatrixMode;
    bool transformRequired;
} DXMatrices;

typedef struct {
    unsigned int defaultTextureId;
    unsigned int defaultShaderId;
    unsigned int defaultLineShaderId;
    rlRenderBatch defaultBatch;
    rlRenderBatch *currentBatch;
    DXMatrices matrices;
    int vertexCounter;
    float texcoordx, texcoordy;
    float normalx, normaly, normalz;
    unsigned char colorr, colorg, colorb, colora;
    ConstantBuffer constantBuffer;
    int width;
    int height;
    int viewportx, viewporty, viewportWidth, viewportHeight;
    int scissorx, scissory, scissorWidth, scissorHeight;
} DXState;

//----------------------------------------------------------------------------------
// Variables
//----------------------------------------------------------------------------------

static const int constantBufferIndex = NUM_DESCRIPTORS - 1;
static DriverData driver = { 0 };
static DXState dxState = { 0 };

//----------------------------------------------------------------------------------
// Utility functions
//----------------------------------------------------------------------------------

static Vector VectorCreate(size_t elementSize)
{
    Vector result = { 0 };
    result.elementSize = elementSize;
    result.length = 0;
    result.capacity = 1;
    result.data = (unsigned char*)RL_MALLOC(elementSize * result.capacity);
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
        RL_FREE(vector->data);
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
    vector->data = RL_REALLOC(vector->data, vector->elementSize * capacity);
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to retrieve description for adaptar!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to create DXGI Factory!");
        return false;
    }

    IDXGIFactory7* queryFactory = NULL;
    result = factory->lpVtbl->QueryInterface(factory, &IID_IDXGIFactory7, (LPVOID*)&queryFactory);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to query factory interface!\n");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to query IUnknown for IDXGIAdapter!");
        return false;
    }

    result = D3D12CreateDevice(unknownAdapter, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device9, (LPVOID*)&driver.device);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create device!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to create command queue!");
        return false;
    }

    result = driver.device->lpVtbl->CreateCommandAllocator(driver.device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (LPVOID*)&driver.commandAllocator);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create command allocator!");
        return false;
    }

    result = driver.device->lpVtbl->CreateCommandList(driver.device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, driver.commandAllocator, NULL, &IID_ID3D12GraphicsCommandList1, (LPVOID*)&driver.commandList);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create command list!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to serialize versioned root signature! Error: %s", (LPCSTR)error->lpVtbl->GetBufferPointer(error));
        return false;
    }

    result = driver.device->lpVtbl->CreateRootSignature(driver.device, 0, signature->lpVtbl->GetBufferPointer(signature),
        signature->lpVtbl->GetBufferSize(signature), &IID_ID3D12RootSignature, (LPVOID*)&driver.rootSignature);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create root signature!");
        return false;
    }

    return true;
}

static void BindRootSignature()
{
    driver.commandList->lpVtbl->SetGraphicsRootSignature(driver.commandList, driver.rootSignature);
}

static bool InitializeFence()
{
    HRESULT result = driver.device->lpVtbl->CreateFence(driver.device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (LPVOID*)&driver.fence);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create fence!");
        return false;
    }
    driver.fenceValue = 0;

    driver.fenceEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (driver.fenceEvent == NULL)
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create fence event!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to retrieve buffer for render target index: %d!", index);
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to create render target descriptors!");
        return false;
    }

    IUnknown* unknownCommandQueue = NULL;
    HRESULT result = driver.commandQueue->lpVtbl->QueryInterface(driver.commandQueue, &IID_IUnknown, (LPVOID*)&unknownCommandQueue);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to query for IUnknown command queue!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to create swap chain!");
        return false;
    }

    result = driver.factory->lpVtbl->MakeWindowAssociation(driver.factory, handle, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to make window association!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to create constant buffer resource!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to map constant buffer memory!");
        return false;
    }

    return true;
}

static bool InitializeDepthStencil(int width, int height)
{
    if (!CreateDescriptorHeap(&driver.depthStencil.descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create depth stencil descriptor heap!");
        return false;
    }

    D3D12_HEAP_PROPERTIES heap = { 0 };
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource = { 0 };
    resource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource.Alignment = 0;
    resource.Width = (UINT64)width;
    resource.Height = (UINT64)height;
    resource.DepthOrArraySize = 1;
    resource.MipLevels = 0;
    resource.Format = DXGI_FORMAT_D32_FLOAT;
    resource.SampleDesc.Count = 1;
    resource.SampleDesc.Quality = 0;
    resource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = { 0 };
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE clearValue = { 0 };
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &heap, D3D12_HEAP_FLAG_NONE, &resource, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, &IID_ID3D12Resource, (LPVOID*)&driver.depthStencil.resource);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create depth stencil resource!");
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE offset = CPUOffset(&driver.depthStencil.descriptor, 0);
    driver.device->lpVtbl->CreateDepthStencilView(driver.device, driver.depthStencil.resource, &dsvDesc, offset);

    return true;
}

#if defined(DIRECTX_INFOQUEUE)
static bool InitializeInfoQueue()
{
    if (FAILED(driver.device->lpVtbl->QueryInterface(driver.device, &IID_ID3D12InfoQueue, (LPVOID*)&driver.infoQueue)))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to initialize info queue!");
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

        D3D12_MESSAGE* message = (D3D12_MESSAGE*)RL_MALLOC(length);
        if (SUCCEEDED(driver.infoQueue->lpVtbl->GetMessage(driver.infoQueue, i, message, &length)))
        {
            DXTRACELOG(RL_LOG_INFO, "%s", message->pDescription);
        }
        RL_FREE(message);
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

static void SetRenderTargets()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CPUOffset(&driver.rtv, driver.frameIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = CPUOffset(&driver.depthStencil.descriptor, 0);
    driver.commandList->lpVtbl->OMSetRenderTargets(driver.commandList, 1, &rtvHandle, FALSE, &dsvHandle);
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

    SetRenderTargets();

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = CPUOffset(&driver.depthStencil.descriptor, 0);
    driver.commandList->lpVtbl->ClearDepthStencilView(driver.commandList, dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
}

static void SetViewport()
{
    D3D12_VIEWPORT viewport = { 0 };
    viewport.TopLeftX = (FLOAT)dxState.viewportx;
    viewport.TopLeftY = (FLOAT)dxState.viewporty;
    viewport.Width = (FLOAT)dxState.viewportWidth;
    viewport.Height = (FLOAT)dxState.viewportHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    driver.commandList->lpVtbl->RSSetViewports(driver.commandList, 1, &viewport);
}

static void SetScissor()
{
    D3D12_RECT scissor = { 0 };
    scissor.left = dxState.scissorx;
    scissor.top = dxState.scissory;
    scissor.right = scissor.left + dxState.scissorWidth;
    scissor.bottom = scissor.top + dxState.scissorHeight;
    driver.commandList->lpVtbl->RSSetScissorRects(driver.commandList, 1, &scissor);
}

static DXShader *GetShader(unsigned int id)
{
    for (size_t i = 0; i < driver.shaders.pool.length; i++)
    {
        DXShader *shader = (DXShader*)VectorGet(&driver.shaders.pool, i);

        if (shader->id == id)
        {
            return shader;
        }
    }

    return NULL;
}

static bool RemoveShader(unsigned int id)
{
    for (size_t i = 0; i < driver.shaders.pool.length; i++)
    {
        DXShader *shader = (DXShader*)VectorGet(&driver.shaders.pool, i);

        if (shader->id == id)
        {
            VectorRemove(&driver.shaders.pool, i);
            return true;
        }
    }

    return false;
}

static unsigned int CreatePipeline(unsigned int vShaderId, unsigned int fShaderId, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
{
    DXShader *vShader = GetShader(vShaderId);
    DXShader *fShader = GetShader(fShaderId);

    if (vShader == NULL)
    {
        DXTRACELOG(RL_LOG_ERROR, "Invalid vertex shader id '%d'!", vShaderId);
        return 0;
    }

    if (vShader->type != RL_VERTEX_SHADER)
    {
        DXTRACELOG(RL_LOG_ERROR, "Vertex shader '%d' is not RL_VERTEX_SHADER!", vShaderId);
        return 0;
    }

    if (fShader == NULL)
    {
        DXTRACELOG(RL_LOG_ERROR, "Invalid fragment shader id '%d'!", fShaderId);
        return 0;
    }

    if (fShader->type != RL_FRAGMENT_SHADER)
    {
        DXTRACELOG(RL_LOG_ERROR, "Fragment shader '%d' is not RL_FRAGMENT_SHADER!", fShaderId);
        return 0;
    }

    D3D12_INPUT_ELEMENT_DESC elements[4] = { {0}, {0}, {0}, {0} };
    elements[0].SemanticName = "POSITION";
    elements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    elements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    elements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;

    elements[1].SemanticName = "TEXCOORD";
    elements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    elements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    elements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    elements[1].InputSlot = 1;

    elements[2].SemanticName = "NORMAL";
    elements[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    elements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    elements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    elements[2].InputSlot = 2;

    elements[3].SemanticName = "COLOR";
    elements[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    elements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    elements[3].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    elements[3].InputSlot = 3;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc = { 0 };
    graphicsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    graphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    graphicsDesc.RasterizerState.FrontCounterClockwise = TRUE;
    graphicsDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    graphicsDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    graphicsDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    graphicsDesc.RasterizerState.DepthClipEnable = TRUE;
    graphicsDesc.RasterizerState.MultisampleEnable = TRUE;
    graphicsDesc.RasterizerState.AntialiasedLineEnable = TRUE;
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
    graphicsDesc.PrimitiveTopologyType = topology;
    graphicsDesc.NumRenderTargets = 1;
    graphicsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsDesc.SampleDesc.Count = 1;

    graphicsDesc.InputLayout.pInputElementDescs = elements;
    graphicsDesc.InputLayout.NumElements = _countof(elements);
    graphicsDesc.pRootSignature = driver.rootSignature;
    graphicsDesc.VS.pShaderBytecode = vShader->data->lpVtbl->GetBufferPointer(vShader->data);
    graphicsDesc.VS.BytecodeLength = vShader->data->lpVtbl->GetBufferSize(vShader->data);
    graphicsDesc.PS.pShaderBytecode = fShader->data->lpVtbl->GetBufferPointer(fShader->data);
    graphicsDesc.PS.BytecodeLength = fShader->data->lpVtbl->GetBufferSize(fShader->data);

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

    DXPipeline pipeline = { 0 };
    HRESULT result = driver.device->lpVtbl->CreateGraphicsPipelineState(driver.device, &graphicsDesc, &IID_ID3D12PipelineState, (LPVOID*)&pipeline.state);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to load shader program!");
    }
    else
    {
        pipeline.id = driver.pipelines.index++;
        VectorPush(&driver.pipelines.pool, &pipeline);
    }

    DXRELEASE(vShader->data);
    DXRELEASE(vShader->data);

    RemoveShader(vShader->id);
    RemoveShader(fShader->id);

    return pipeline.id;
}

static DXPipeline *GetPipeline(unsigned int id)
{
    for (size_t i = 0; i < driver.pipelines.pool.length; i++)
    {
        DXPipeline *pipeline = (DXPipeline*)VectorGet(&driver.pipelines.pool, i);

        if (pipeline->id == id)
        {
            return pipeline;
        }
    }

    return NULL;
}

static void BindPipeline(unsigned int id)
{
    DXPipeline *pipeline = GetPipeline(id);

    if (pipeline != NULL)
    {
        driver.commandList->lpVtbl->SetPipelineState(driver.commandList, pipeline->state);
    }
}

static bool InitializeDefaultShader()
{
    const char *vertexShaderCode =
    "struct PSInput                         \n"
    "{                                      \n"
    "   float4 position : SV_POSITION;      \n"
    "   float2 uv : TEXCOORD;               \n"
    "   float4 color : COLOR;               \n"
    "};                                     \n"

    "cbuffer ConstantBuffer : register(b0)  \n"
    "{                                      \n"
    "   float4x4 mvp;                       \n"
    "   int pad[6];                         \n"
    "}                                      \n"

    "PSInput Main(float4 position : POSITION, float2 uv : TEXCOORD, float4 normal : NORMAL, float4 color : COLOR)  \n"
    "{                                                                                                             \n"
    "   PSInput result;                                                                                            \n"
    "   result.position = mul(mvp, float4(position.xyz, 1.0));                                                     \n"
    "   result.uv = uv;                                                                                            \n"
    "   result.color = color;                                                                                      \n"
    "   return result;                                                                                             \n"
    "}";

    const char *fragmentShaderCode =
    "struct PSInput                         \n"
    "{                                      \n"
    "   float4 position : SV_POSITION;      \n"
    "   float2 uv : TEXCOORD;               \n"
    "   float4 color : COLOR;               \n"
    "};                                     \n"
    "Texture2D g_Texture : register(t0);    \n"
    "SamplerState g_Sampler : register(s0); \n"
    "float4 Main(PSInput input) : SV_TARGET \n"
    "{                                      \n"
    "   return g_Texture.Sample(g_Sampler, input.uv) * input.color; \n"
    "}";

    dxState.defaultShaderId = rlLoadShaderCode(vertexShaderCode, fragmentShaderCode);

    unsigned int vShaderId = rlCompileShader(vertexShaderCode, RL_VERTEX_SHADER);
    unsigned int fShaderId = rlCompileShader(fragmentShaderCode, RL_FRAGMENT_SHADER);
    dxState.defaultLineShaderId = CreatePipeline(vShaderId, fShaderId, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

    return dxState.defaultShaderId != 0 && dxState.defaultLineShaderId;
}

static DXVertexBuffer CreateVertexBuffer(UINT64 size, UINT stride)
{
    DXVertexBuffer buffer = { 0 };

    D3D12_HEAP_PROPERTIES bufferHeap = { 0 };
    bufferHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    bufferHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    bufferHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    bufferHeap.CreationNodeMask = 1;
    bufferHeap.VisibleNodeMask = 1;

    D3D12_HEAP_PROPERTIES uploadBufferHeap = { 0 };
    uploadBufferHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadBufferHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadBufferHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadBufferHeap.CreationNodeMask = 1;
    uploadBufferHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource = { 0 };
    resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource.Alignment = 0;
    resource.Width = size;
    resource.Height = 1;
    resource.DepthOrArraySize = 1;
    resource.MipLevels = 1;
    resource.Format = DXGI_FORMAT_UNKNOWN;
    resource.SampleDesc.Count = 1;
    resource.SampleDesc.Quality = 0;
    resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &bufferHeap, D3D12_HEAP_FLAG_NONE, &resource, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource, (LPVOID*)&buffer.buffer);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create default heap buffer!");
        return buffer;
    }

    result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &uploadBufferHeap, D3D12_HEAP_FLAG_NONE, &resource, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (LPVOID*)&buffer.uploadBuffer);
    if (FAILED(result))
    {
        DXRELEASE(buffer.buffer);
        DXTRACELOG(RL_LOG_ERROR, "Failed to create upload heap buffer!");
        return buffer;
    }

    buffer.view.BufferLocation = buffer.buffer->lpVtbl->GetGPUVirtualAddress(buffer.buffer);
    buffer.view.StrideInBytes = stride;

    return buffer;
}

static void DestroyVertexBuffer(DXVertexBuffer *buffer)
{
    DXRELEASE(buffer->buffer);
    DXRELEASE(buffer->uploadBuffer);
}

static void DestroyRenderBuffer(DXRenderBuffer *renderBuffer)
{
    DestroyVertexBuffer(&renderBuffer->vertex);
    DestroyVertexBuffer(&renderBuffer->texcoord);
    DestroyVertexBuffer(&renderBuffer->normal);
    DestroyVertexBuffer(&renderBuffer->color);
    DXRELEASE(renderBuffer->index);
}

static unsigned int CreateRenderBuffer(
    UINT64 vertexBufferSize, UINT vertexBufferStride,
    UINT64 texcoordBufferSize, UINT texcoordBufferStride,
    UINT64 normalBufferSize, UINT normalBufferStride,
    UINT64 colorBufferSize, UINT colorBufferStride,
    UINT64 indexBufferSize)
{
    DXRenderBuffer buffer = { 0 };
    buffer.vertex = CreateVertexBuffer(vertexBufferSize, vertexBufferStride);
    if (buffer.vertex.buffer == NULL)
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create vertex buffer resource!");
        return 0;
    }

    buffer.texcoord = CreateVertexBuffer(texcoordBufferSize, texcoordBufferStride);
    if (buffer.texcoord.buffer == NULL)
    {
        DestroyRenderBuffer(&buffer);
        DXTRACELOG(RL_LOG_ERROR, "Failed to create texcoord buffer resource!");
        return 0;
    }

    buffer.normal = CreateVertexBuffer(normalBufferSize, normalBufferStride);
    if (buffer.normal.buffer == NULL)
    {
        DestroyRenderBuffer(&buffer);
        DXTRACELOG(RL_LOG_ERROR, "Failed to create normal buffer resource!");
        return 0;
    }

    buffer.color = CreateVertexBuffer(colorBufferSize, colorBufferStride);
    if (buffer.color.buffer == NULL)
    {
        DestroyRenderBuffer(&buffer);
        DXTRACELOG(RL_LOG_ERROR, "Failed to create color buffer resource!");
        return 0;
    }

    D3D12_HEAP_PROPERTIES heap = { 0 };
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource = { 0 };
    resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource.Alignment = 0;
    resource.Width = indexBufferSize;
    resource.Height = 1;
    resource.DepthOrArraySize = 1;
    resource.MipLevels = 1;
    resource.Format = DXGI_FORMAT_UNKNOWN;
    resource.SampleDesc.Count = 1;
    resource.SampleDesc.Quality = 0;
    resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &heap, D3D12_HEAP_FLAG_NONE, &resource, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &buffer.index);
    if (FAILED(result))
    {
        DestroyRenderBuffer(&buffer);
        DXTRACELOG(RL_LOG_ERROR, "Failed to create index buffer resource!");
        return 0;
    }

    buffer.indexView.BufferLocation = buffer.index->lpVtbl->GetGPUVirtualAddress(buffer.index);
    buffer.indexView.Format = DXGI_FORMAT_R32_UINT;

    buffer.id = driver.renderBuffers.index++;
    VectorPush(&driver.renderBuffers.pool, &buffer);

    return buffer.id;
}

static DXRenderBuffer *GetRenderBuffer(unsigned int id)
{
    for (size_t i = 0; i < driver.renderBuffers.pool.length; i++)
    {
        DXRenderBuffer *buffer = (DXRenderBuffer*)VectorGet(&driver.renderBuffers.pool, i);

        if (buffer->id == id)
        {
            return buffer;
        }
    }

    return NULL;
}

static bool PrepUploadData(DXVertexBuffer *buffer, void *data, size_t size)
{
    unsigned char *bufferData = NULL;
    D3D12_RANGE range = { 0 };

    HRESULT result = buffer->uploadBuffer->lpVtbl->Map(buffer->uploadBuffer, 0, &range, (LPVOID*)&bufferData);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_WARNING, "Failed to map resource for upload!");
        return false;
    }

    memcpy(bufferData, data, size);

    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = buffer->buffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    driver.commandList->lpVtbl->ResourceBarrier(driver.commandList, 1, &barrier);
    driver.commandList->lpVtbl->CopyBufferRegion(driver.commandList, buffer->buffer, 0, buffer->uploadBuffer, 0, size);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    driver.commandList->lpVtbl->ResourceBarrier(driver.commandList, 1, &barrier);

    buffer->view.SizeInBytes = (UINT)size;

    return true;
}

static Matrix rlMatrixIdentity(void)
{
    Matrix result = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    return result;
}

// Get two matrix multiplication
// NOTE: When multiplying matrices... the order matters!
static Matrix rlMatrixMultiply(Matrix left, Matrix right)
{
    Matrix result = { 0 };

    result.m0 = left.m0*right.m0 + left.m1*right.m4 + left.m2*right.m8 + left.m3*right.m12;
    result.m1 = left.m0*right.m1 + left.m1*right.m5 + left.m2*right.m9 + left.m3*right.m13;
    result.m2 = left.m0*right.m2 + left.m1*right.m6 + left.m2*right.m10 + left.m3*right.m14;
    result.m3 = left.m0*right.m3 + left.m1*right.m7 + left.m2*right.m11 + left.m3*right.m15;
    result.m4 = left.m4*right.m0 + left.m5*right.m4 + left.m6*right.m8 + left.m7*right.m12;
    result.m5 = left.m4*right.m1 + left.m5*right.m5 + left.m6*right.m9 + left.m7*right.m13;
    result.m6 = left.m4*right.m2 + left.m5*right.m6 + left.m6*right.m10 + left.m7*right.m14;
    result.m7 = left.m4*right.m3 + left.m5*right.m7 + left.m6*right.m11 + left.m7*right.m15;
    result.m8 = left.m8*right.m0 + left.m9*right.m4 + left.m10*right.m8 + left.m11*right.m12;
    result.m9 = left.m8*right.m1 + left.m9*right.m5 + left.m10*right.m9 + left.m11*right.m13;
    result.m10 = left.m8*right.m2 + left.m9*right.m6 + left.m10*right.m10 + left.m11*right.m14;
    result.m11 = left.m8*right.m3 + left.m9*right.m7 + left.m10*right.m11 + left.m11*right.m15;
    result.m12 = left.m12*right.m0 + left.m13*right.m4 + left.m14*right.m8 + left.m15*right.m12;
    result.m13 = left.m12*right.m1 + left.m13*right.m5 + left.m14*right.m9 + left.m15*right.m13;
    result.m14 = left.m12*right.m2 + left.m13*right.m6 + left.m14*right.m10 + left.m15*right.m14;
    result.m15 = left.m12*right.m3 + left.m13*right.m7 + left.m14*right.m11 + left.m15*right.m15;

    return result;
}

static Matrix rlMatrixTranspose(Matrix mat)
{
    Matrix result = { 0 };

    result.m0 = mat.m0;
    result.m1 = mat.m4;
    result.m2 = mat.m8;
    result.m3 = mat.m12;

    result.m4 = mat.m1;
    result.m5 = mat.m5;
    result.m6 = mat.m9;
    result.m7 = mat.m13;

    result.m8 = mat.m2;
    result.m9 = mat.m6;
    result.m10 = mat.m10;
    result.m11 = mat.m14;

    result.m12 = mat.m3;
    result.m13 = mat.m7;
    result.m14 = mat.m11;
    result.m15 = mat.m15;

    return result;
}

static DXTexture *GetTexture(unsigned int id)
{
    for (size_t i = 0; i < driver.textures.pool.length; i++)
    {
        DXTexture *texture = (DXTexture*)VectorGet(&driver.textures.pool, i);

        if (texture->id == id)
        {
            return texture;
        }
    }

    return NULL;
}

static void BindTexture(unsigned int id)
{
    DXTexture *texture = GetTexture(id);
    D3D12_GPU_DESCRIPTOR_HANDLE offset = GPUOffset(&driver.srv, texture->offset);
    driver.commandList->lpVtbl->SetGraphicsRootDescriptorTable(driver.commandList, 0, offset);
}

static DXGI_FORMAT ToDXGIFormat(rlPixelFormat format)
{
    switch (format)
    {
    case RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: return DXGI_FORMAT_B4G4R4A4_UNORM;
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8:
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    default: break;
    }

    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

static int StrideInBytes(rlPixelFormat format)
{
    switch (format)
    {
    case RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: return 2;
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8: return 3;
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: return 4;
    default: break;
    }

    return 4;
}

//----------------------------------------------------------------------------------
// API
//----------------------------------------------------------------------------------

void rlMatrixMode(int mode)
{
    if (mode == RL_PROJECTION)
    {
        dxState.matrices.currentMatrix = &dxState.matrices.projection;
    }
    else if (mode == RL_MODELVIEW)
    {
        dxState.matrices.currentMatrix = &dxState.matrices.modelView;
    }

    dxState.matrices.currentMatrixMode = mode;
}

void rlPushMatrix(void)
{
    if (dxState.matrices.stackCounter >= RL_MAX_MATRIX_STACK_SIZE)
    {
        DXTRACELOG(RL_LOG_ERROR, "Matrix stack overflow!");
        return;
    }

    if (dxState.matrices.currentMatrixMode == RL_MODELVIEW)
    {
        dxState.matrices.transformRequired = true;
        dxState.matrices.currentMatrix = &dxState.matrices.transform;
    }

    dxState.matrices.stack[dxState.matrices.stackCounter] = *dxState.matrices.currentMatrix;
    dxState.matrices.stackCounter++;
}

void rlPopMatrix(void)
{
    if (dxState.matrices.stackCounter > 0)
    {
        Matrix mat = dxState.matrices.stack[dxState.matrices.stackCounter - 1];
        *dxState.matrices.currentMatrix = mat;
        dxState.matrices.stackCounter--;
    }

    if ((dxState.matrices.stackCounter == 0) && (dxState.matrices.currentMatrixMode  == RL_MODELVIEW))
    {
        dxState.matrices.currentMatrix = &dxState.matrices.modelView;
        dxState.matrices.transformRequired = false;
    }
}

void rlLoadIdentity(void)
{
    *dxState.matrices.currentMatrix = rlMatrixIdentity();
}

void rlTranslatef(float x, float y, float z)
{
    Matrix matTranslation = {
        1.0f, 0.0f, 0.0f, x,
        0.0f, 1.0f, 0.0f, y,
        0.0f, 0.0f, 1.0f, z,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // NOTE: We transpose matrix with multiplication order
    *dxState.matrices.currentMatrix = rlMatrixMultiply(matTranslation, *dxState.matrices.currentMatrix);
}

void rlRotatef(float angle, float x, float y, float z)
{
    Matrix matRotation = rlMatrixIdentity();

    // Axis vector (x, y, z) normalization
    float lengthSquared = x*x + y*y + z*z;
    if ((lengthSquared != 1.0f) && (lengthSquared != 0.0f))
    {
        float inverseLength = 1.0f/sqrtf(lengthSquared);
        x *= inverseLength;
        y *= inverseLength;
        z *= inverseLength;
    }

    // Rotation matrix generation
    float sinres = sinf(DEG2RAD*angle);
    float cosres = cosf(DEG2RAD*angle);
    float t = 1.0f - cosres;

    matRotation.m0 = x*x*t + cosres;
    matRotation.m1 = y*x*t + z*sinres;
    matRotation.m2 = z*x*t - y*sinres;
    matRotation.m3 = 0.0f;

    matRotation.m4 = x*y*t - z*sinres;
    matRotation.m5 = y*y*t + cosres;
    matRotation.m6 = z*y*t + x*sinres;
    matRotation.m7 = 0.0f;

    matRotation.m8 = x*z*t + y*sinres;
    matRotation.m9 = y*z*t - x*sinres;
    matRotation.m10 = z*z*t + cosres;
    matRotation.m11 = 0.0f;

    matRotation.m12 = 0.0f;
    matRotation.m13 = 0.0f;
    matRotation.m14 = 0.0f;
    matRotation.m15 = 1.0f;

    // NOTE: We transpose matrix with multiplication order
    *dxState.matrices.currentMatrix = rlMatrixMultiply(matRotation, *dxState.matrices.currentMatrix);
}

void rlScalef(float x, float y, float z)
{
    Matrix matScale = {
        x, 0.0f, 0.0f, 0.0f,
        0.0f, y, 0.0f, 0.0f,
        0.0f, 0.0f, z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // NOTE: We transpose matrix with multiplication order
    *dxState.matrices.currentMatrix = rlMatrixMultiply(matScale, *dxState.matrices.currentMatrix);
}

void rlMultMatrixf(const float *matf)
{
    // Matrix creation from array
    Matrix mat = { matf[0], matf[4], matf[8], matf[12],
                   matf[1], matf[5], matf[9], matf[13],
                   matf[2], matf[6], matf[10], matf[14],
                   matf[3], matf[7], matf[11], matf[15] };

    *dxState.matrices.currentMatrix = rlMatrixMultiply(mat, *dxState.matrices.currentMatrix);
}

void rlFrustum(double left, double right, double bottom, double top, double znear, double zfar)
{
    Matrix matFrustum = { 0 };

    float rl = (float)(right - left);
    float tb = (float)(top - bottom);
    float fn = (float)(zfar - znear);

    matFrustum.m0 = ((float) znear*2.0f)/rl;
    matFrustum.m1 = 0.0f;
    matFrustum.m2 = 0.0f;
    matFrustum.m3 = 0.0f;

    matFrustum.m4 = 0.0f;
    matFrustum.m5 = ((float) znear*2.0f)/tb;
    matFrustum.m6 = 0.0f;
    matFrustum.m7 = 0.0f;

    matFrustum.m8 = ((float)right + (float)left)/rl;
    matFrustum.m9 = ((float)top + (float)bottom)/tb;
    matFrustum.m10 = -((float)zfar + (float)znear)/fn;
    matFrustum.m11 = -1.0f;

    matFrustum.m12 = 0.0f;
    matFrustum.m13 = 0.0f;
    matFrustum.m14 = -((float)zfar*(float)znear*2.0f)/fn;
    matFrustum.m15 = 0.0f;

    *dxState.matrices.currentMatrix = rlMatrixMultiply(*dxState.matrices.currentMatrix, matFrustum);
}

void rlOrtho(double left, double right, double bottom, double top, double znear, double zfar)
{
    // NOTE: If left-right and top-botton values are equal it could create a division by zero,
    // response to it is platform/compiler dependant
    Matrix matOrtho = { 0 };

    const float l = (float)left;
    const float r = (float)right;
    const float t = (float)top;
    const float b = (float)bottom;
    const float n = (float)znear;
    const float f = (float)zfar;

    matOrtho.m0 = 2.0f / (r - l);
    matOrtho.m1 = 0.0f;
    matOrtho.m2 = 0.0f;
    matOrtho.m3 = 0.0f;

    matOrtho.m4 = 0.0f;
    matOrtho.m5 = 2.0f / (t - b);
    matOrtho.m6 = 0.0f;
    matOrtho.m7 = 0.0f;

    matOrtho.m8 = 0.0f;
    matOrtho.m9 = 0.0f;
    matOrtho.m10 = 1.0f / (n - f);
    matOrtho.m11 = 0.0f;

    matOrtho.m12 = (l + r) / (l - r);
    matOrtho.m13 = (b + t) / (b - t);
    matOrtho.m14 = n / (n - f);
    matOrtho.m15 = 1.0f;

    *dxState.matrices.currentMatrix = rlMatrixMultiply(*dxState.matrices.currentMatrix, matOrtho);
}

void rlViewport(int x, int y, int width, int height)
{
    dxState.viewportx = x;
    dxState.viewporty = y;
    dxState.viewportWidth = width;
    dxState.viewportHeight = height;
}

void rlSetClipPlanes(double nearPlane, double farPlane) {}
double rlGetCullDistanceNear(void) { return 0.0; }
double rlGetCullDistanceFar(void) { return 0.0; }

//------------------------------------------------------------------------------------
// Functions Declaration - Vertex level operations
//------------------------------------------------------------------------------------
void rlBegin(int mode)
{
    if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode != mode)
    {
        if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount > 0)
        {
            if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode == RL_LINES) dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment = ((dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount < 4)? dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount : dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount%4);
            else if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode == RL_TRIANGLES) dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment = ((dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount < 4)? 1 : (4 - (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount%4)));
            else dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment = 0;

            if (!rlCheckRenderBatchLimit(dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment))
            {
                dxState.vertexCounter += dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment;
                dxState.currentBatch->drawCounter++;
            }
        }

        if (dxState.currentBatch->drawCounter >= RL_DEFAULT_BATCH_DRAWCALLS) rlDrawRenderBatch(dxState.currentBatch);

        dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode = mode;
        dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount = 0;
        dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].textureId = dxState.defaultTextureId;
    }
}

void rlEnd(void)
{
    dxState.currentBatch->currentDepth += (1.0f/20000.0f);
}

void rlVertex2i(int x, int y)
{
    rlVertex3f((float)x, (float)y, dxState.currentBatch->currentDepth);
}

void rlVertex2f(float x, float y)
{
    rlVertex3f(x, y, dxState.currentBatch->currentDepth);
}

void rlVertex3f(float x, float y, float z)
{
    float tx = x;
    float ty = y;
    float tz = z;

    // Transform provided vector if required
    if (dxState.matrices.transformRequired)
    {
        tx = dxState.matrices.transform.m0*x + dxState.matrices.transform.m4*y + dxState.matrices.transform.m8*z + dxState.matrices.transform.m12;
        ty = dxState.matrices.transform.m1*x + dxState.matrices.transform.m5*y + dxState.matrices.transform.m9*z + dxState.matrices.transform.m13;
        tz = dxState.matrices.transform.m2*x + dxState.matrices.transform.m6*y + dxState.matrices.transform.m10*z + dxState.matrices.transform.m14;
    }

    // WARNING: We can't break primitives when launching a new batch.
    // RL_LINES comes in pairs, RL_TRIANGLES come in groups of 3 vertices and RL_QUADS come in groups of 4 vertices.
    // We must check current draw.mode when a new vertex is required and finish the batch only if the draw.mode draw.vertexCount is %2, %3 or %4
    if (dxState.vertexCounter > (dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].elementCount*4 - 4))
    {
        if ((dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode == RL_LINES) &&
            (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount%2 == 0))
        {
            // Reached the maximum number of vertices for RL_LINES drawing
            // Launch a draw call but keep current state for next vertices comming
            // NOTE: We add +1 vertex to the check for security
            rlCheckRenderBatchLimit(2 + 1);
        }
        else if ((dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode == RL_TRIANGLES) &&
            (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount%3 == 0))
        {
            rlCheckRenderBatchLimit(3 + 1);
        }
        else if ((dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode == RL_QUADS) &&
            (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount%4 == 0))
        {
            rlCheckRenderBatchLimit(4 + 1);
        }
    }

    // Add vertices
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].vertices[3*dxState.vertexCounter] = tx;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].vertices[3*dxState.vertexCounter + 1] = ty;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].vertices[3*dxState.vertexCounter + 2] = tz;

    // Add current texcoord
    const int drawCounter = dxState.currentBatch->drawCounter;
    DXTexture *texture = GetTexture(dxState.currentBatch->draws[drawCounter].textureId);
    const float texcoordx = (float)dxState.texcoordx / (float)texture->width;
    const float texcoordy = (float)dxState.texcoordy / (float)texture->height;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].texcoords[2*dxState.vertexCounter] = texcoordx;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].texcoords[2*dxState.vertexCounter + 1] = texcoordy;

    // Add current normal
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].normals[3*dxState.vertexCounter] = dxState.normalx;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].normals[3*dxState.vertexCounter + 1] = dxState.normaly;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].normals[3*dxState.vertexCounter + 2] = dxState.normalz;

    // Add current color
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].colors[4*dxState.vertexCounter] = dxState.colorr;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].colors[4*dxState.vertexCounter + 1] = dxState.colorg;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].colors[4*dxState.vertexCounter + 2] = dxState.colorb;
    dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].colors[4*dxState.vertexCounter + 3] = dxState.colora;

    dxState.vertexCounter++;
    dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount++;
}

void rlTexCoord2f(float x, float y)
{
    dxState.texcoordx = x;
    dxState.texcoordy = y;
}

void rlNormal3f(float x, float y, float z)
{
    float normalx = x;
    float normaly = y;
    float normalz = z;
    if (dxState.matrices.transformRequired)
    {
        normalx = dxState.matrices.transform.m0*x + dxState.matrices.transform.m4*y + dxState.matrices.transform.m8*z;
        normaly = dxState.matrices.transform.m1*x + dxState.matrices.transform.m5*y + dxState.matrices.transform.m9*z;
        normalz = dxState.matrices.transform.m2*x + dxState.matrices.transform.m6*y + dxState.matrices.transform.m10*z;
    }
    float length = sqrtf(normalx*normalx + normaly*normaly + normalz*normalz);
    if (length != 0.0f)
    {
        float ilength = 1.0f/length;
        normalx *= ilength;
        normaly *= ilength;
        normalz *= ilength;
    }
    dxState.normalx = normalx;
    dxState.normaly = normaly;
    dxState.normalz = normalz;
}

void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    dxState.colorr = r;
    dxState.colorg = g;
    dxState.colorb = b;
    dxState.colora = a;
}

void rlColor3f(float x, float y, float z)
{
    rlColor4ub((unsigned char)(x*255.0f), (unsigned char)(y*255.0f), (unsigned char)(z*255.0f), 255);
}

void rlColor4f(float r, float g, float b, float a)
{
    rlColor4ub((unsigned char)(r*255.0f), (unsigned char)(g*255.0f), (unsigned char)(b*255.0f), (unsigned char)(a*255.0f));
}

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

void rlScissor(int x, int y, int width, int height)
{
    dxState.scissorx = x;
    dxState.scissory = y;
    dxState.scissorWidth = width;
    dxState.scissorHeight = height;
}

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
    driver.textures.pool = VectorCreate(sizeof(DXTexture));
    driver.textures.index = 1;

    driver.renderBuffers.pool = VectorCreate(sizeof(DXRenderBuffer));
    driver.renderBuffers.index = 1;

    driver.pipelines.pool = VectorCreate(sizeof(DXPipeline));
    driver.pipelines.index = 1;

    driver.shaders.pool = VectorCreate(sizeof(DXShader));
    driver.shaders.index = 1;

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
        DXTRACELOG(RL_LOG_ERROR, "Failed to create SRV descriptor heap!");
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

    if (!InitializeDepthStencil(width, height))
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

    DXGI_ADAPTER_DESC1 desc = { 0 };
    if (FAILED(driver.adapter->lpVtbl->GetDesc1(driver.adapter, &desc)))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to retrieve adapter description!");
        return;
    }

    DXTRACELOG(RL_LOG_INFO, "Initialized DirectX!");

    char* driverName = Windows_ToMultiByte(desc.Description);
    DXTRACELOG(RL_LOG_INFO, "Driver is %s.", driverName);
    RL_FREE(driverName);

    // Init default white texture
    unsigned char pixels[4] = { 255, 255, 255, 255 };   // 1 pixel RGBA (4 bytes)
    dxState.defaultTextureId = rlLoadTexture(pixels, 1, 1, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    dxState.defaultBatch = rlLoadRenderBatch(RL_DEFAULT_BATCH_BUFFERS, RL_DEFAULT_BATCH_BUFFER_ELEMENTS);
    dxState.currentBatch = &dxState.defaultBatch;

    for (int i = 0; i < RL_MAX_MATRIX_STACK_SIZE; i++)
    {
        dxState.matrices.stack[i] = rlMatrixIdentity();
    }

    dxState.matrices.modelView = rlMatrixIdentity();
    dxState.matrices.projection = rlMatrixIdentity();
    dxState.matrices.transform = rlMatrixIdentity();
    dxState.matrices.currentMatrix = &dxState.matrices.modelView;
    dxState.matrices.stackCounter = 0;
    dxState.matrices.currentMatrixMode = 0;

    dxState.width = width;
    dxState.height = height;

    UpdateRenderTarget();
    rlViewport(0, 0, width, height);
    rlScissor(0, 0, width, height);
}

void rlglClose(void)
{
    rlUnloadRenderBatch(dxState.defaultBatch);

    for (size_t i = 0; i < driver.shaders.pool.length; i++)
    {
        DXShader *shader = (DXShader*)VectorGet(&driver.shaders.pool, i);
        DXRELEASE(shader->data);
    }
    VectorDestroy(&driver.shaders.pool);

    for (size_t i = 0; i < driver.pipelines.pool.length; i++)
    {
        DXPipeline *pipeline = (DXPipeline*)VectorGet(&driver.pipelines.pool, i);
        DXRELEASE(pipeline->state);
    }
    VectorDestroy(&driver.pipelines.pool);

    for (size_t i = 0; i < driver.textures.pool.length; i++)
    {
        DXTexture *texture = (DXTexture*)VectorGet(&driver.textures.pool, i);
        DXRELEASE(texture->data);
        DXRELEASE(texture->upload);
    }
    VectorDestroy(&driver.textures.pool);

    for (size_t i = 0; i < driver.renderBuffers.pool.length; i++)
    {
        DXRenderBuffer* renderBuffer = (DXRenderBuffer*)VectorGet(&driver.renderBuffers.pool, i);
        DestroyRenderBuffer(renderBuffer);
    }
    VectorDestroy(&driver.renderBuffers.pool);

    DXRELEASE(driver.constantBuffer);
    DXRELEASE(driver.renderTargets[0]);
    DXRELEASE(driver.renderTargets[1]);
    DXRELEASE(driver.depthStencil.resource);
    DXRELEASE(driver.depthStencil.descriptor.descriptorHeap);
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

unsigned int rlGetTextureIdDefault(void)
{
    return dxState.defaultTextureId;
}

unsigned int rlGetShaderIdDefault(void) { return 0; }
int *rlGetShaderLocsDefault(void) { return 0; }

// Render batch management
rlRenderBatch rlLoadRenderBatch(int numBuffers, int bufferElements)
{
    rlRenderBatch batch = { 0 };

    const size_t vertexSize = 3 * 4 * sizeof(float);
    const size_t texcoordSize = 2 * 4 * sizeof(float);
    const size_t normalSize = vertexSize;
    const size_t colorSize = 4 * 4 * sizeof(unsigned char);

    const size_t verticesSize = bufferElements * vertexSize;
    const size_t texcoordsSize = bufferElements * texcoordSize;
    const size_t normalsSize = bufferElements * vertexSize;
    const size_t colorsSize = bufferElements * colorSize;
    const size_t indicesSize = bufferElements * 6 * sizeof(unsigned int);

    batch.vertexBuffer = (rlVertexBuffer*)RL_MALLOC(numBuffers * sizeof(rlVertexBuffer));
    for (int i = 0; i < numBuffers; i++)
    {
        batch.vertexBuffer[i].elementCount = bufferElements;
        batch.vertexBuffer[i].vertices = (float *)RL_MALLOC(verticesSize);        // 3 float by vertex, 4 vertex by quad
        batch.vertexBuffer[i].texcoords = (float *)RL_MALLOC(texcoordsSize);       // 2 float by texcoord, 4 texcoord by quad
        batch.vertexBuffer[i].normals = (float *)RL_MALLOC(normalsSize);        // 3 float by vertex, 4 vertex by quad
        batch.vertexBuffer[i].colors = (unsigned char *)RL_MALLOC(colorsSize);   // 4 float by color, 4 colors by quad
        batch.vertexBuffer[i].indices = (unsigned int *)RL_MALLOC(indicesSize);      // 6 int by quad (indices)

        for (int j = 0; j < (3*4*bufferElements); j++) batch.vertexBuffer[i].vertices[j] = 0.0f;
        for (int j = 0; j < (2*4*bufferElements); j++) batch.vertexBuffer[i].texcoords[j] = 0.0f;
        for (int j = 0; j < (3*4*bufferElements); j++) batch.vertexBuffer[i].normals[j] = 0.0f;
        for (int j = 0; j < (4*4*bufferElements); j++) batch.vertexBuffer[i].colors[j] = 0;

        int k = 0;

        // Indices can be initialized right now
        for (int j = 0; j < (6*bufferElements); j += 6)
        {
            batch.vertexBuffer[i].indices[j] = 4*k;
            batch.vertexBuffer[i].indices[j + 1] = 4*k + 1;
            batch.vertexBuffer[i].indices[j + 2] = 4*k + 2;
            batch.vertexBuffer[i].indices[j + 3] = 4*k;
            batch.vertexBuffer[i].indices[j + 4] = 4*k + 2;
            batch.vertexBuffer[i].indices[j + 5] = 4*k + 3;

            k++;
        }
    }

    for (int i = 0; i < numBuffers; i++)
    {
        batch.vertexBuffer[i].vaoId = CreateRenderBuffer(verticesSize, 3 * sizeof(float), texcoordsSize, 2 * sizeof(float), normalsSize, 3 * sizeof(float), colorsSize, 4 * sizeof(unsigned char), indicesSize);
        DXRenderBuffer *renderBuffer = GetRenderBuffer(batch.vertexBuffer[i].vaoId);

        unsigned char *indexData = NULL;
        D3D12_RANGE range = { 0 };

        HRESULT result = renderBuffer->index->lpVtbl->Map(renderBuffer->index, 0, &range, (LPVOID*)&indexData);
        if (FAILED(result))
        {
            DXTRACELOG(RL_LOG_WARNING, "Failed to map resource for upload!");
        }

        memcpy(indexData, batch.vertexBuffer[i].indices, indicesSize);

        renderBuffer->index->lpVtbl->Unmap(renderBuffer->index, 0, NULL);
        renderBuffer->indexView.SizeInBytes = (UINT)indicesSize;
    }

    // Init draw calls tracking system
    //--------------------------------------------------------------------------------------------
    batch.draws = (rlDrawCall *)RL_MALLOC(RL_DEFAULT_BATCH_DRAWCALLS*sizeof(rlDrawCall));

    for (int i = 0; i < RL_DEFAULT_BATCH_DRAWCALLS; i++)
    {
        batch.draws[i].mode = RL_QUADS;
        batch.draws[i].vertexCount = 0;
        batch.draws[i].vertexAlignment = 0;
        batch.draws[i].textureId = dxState.defaultTextureId;
    }

    batch.bufferCount = numBuffers;    // Record buffer count
    batch.drawCounter = 1;             // Reset draws counter
    batch.currentDepth = -1.0f;         // Reset depth value
    
    return batch;
}

void rlUnloadRenderBatch(rlRenderBatch batch)
{
    for (int i = 0; i < batch.bufferCount; i++)
    {
        rlVertexBuffer *vertexBuffer = &batch.vertexBuffer[i];
        RL_FREE(vertexBuffer->vertices);
        RL_FREE(vertexBuffer->texcoords);
        RL_FREE(vertexBuffer->normals);
        RL_FREE(vertexBuffer->colors);
        RL_FREE(vertexBuffer->indices);

        DXRenderBuffer *renderBuffer = GetRenderBuffer(vertexBuffer->vaoId);

        if (renderBuffer == NULL)
        {
            continue;
        }

        DXRELEASE(renderBuffer->vertex.buffer);
        DXRELEASE(renderBuffer->texcoord.buffer);
        DXRELEASE(renderBuffer->color.buffer);
        DXRELEASE(renderBuffer->index);
    }

    RL_FREE(batch.vertexBuffer);
    RL_FREE(batch.draws);
}

void rlDrawRenderBatch(rlRenderBatch *batch)
{
    rlVertexBuffer *vertexBuffer = &batch->vertexBuffer[batch->currentBuffer];
    DXRenderBuffer *renderBuffer = GetRenderBuffer(vertexBuffer->vaoId);

    if (dxState.vertexCounter > 0)
    {
        PrepUploadData(&renderBuffer->vertex, vertexBuffer->vertices, dxState.vertexCounter * 3 * sizeof(float));
        PrepUploadData(&renderBuffer->texcoord, vertexBuffer->texcoords, dxState.vertexCounter * 2 * sizeof(float));
        PrepUploadData(&renderBuffer->normal, vertexBuffer->normals, dxState.vertexCounter * 3 * sizeof(float));
        PrepUploadData(&renderBuffer->color, vertexBuffer->colors, dxState.vertexCounter * 4 * sizeof(unsigned char));
    }

    SetRenderTargets();
    SetViewport();
    SetScissor();
    BindRootSignature();

    ID3D12DescriptorHeap* heaps[] = { driver.srv.descriptorHeap };
    driver.commandList->lpVtbl->SetDescriptorHeaps(driver.commandList, _countof(heaps), heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE constantBufferOffset = GPUOffset(&driver.srv, constantBufferIndex);
    driver.commandList->lpVtbl->SetGraphicsRootDescriptorTable(driver.commandList, 1, constantBufferOffset);

    Matrix matProjection = dxState.matrices.projection;
    Matrix matModelView = dxState.matrices.modelView;
    Matrix mvp = rlMatrixMultiply(matModelView, matProjection);
    dxState.constantBuffer.mpv = rlMatrixTranspose(mvp);
    memcpy(driver.constantBufferPtr, (LPVOID)&dxState.constantBuffer, sizeof(ConstantBuffer));

    if (dxState.vertexCounter > 0)
    {
        D3D12_VERTEX_BUFFER_VIEW views[4] = { renderBuffer->vertex.view, renderBuffer->texcoord.view, renderBuffer->normal.view, renderBuffer->color.view };
        driver.commandList->lpVtbl->IASetVertexBuffers(driver.commandList, 0, _countof(views), views);
        driver.commandList->lpVtbl->IASetIndexBuffer(driver.commandList, &renderBuffer->indexView);
    }

    int eyeCount = 1;
    for (int eye = 0; eye < eyeCount; eye++)
    {
        if (dxState.vertexCounter > 0)
        {
            for (int i = 0, vertexOffset = 0; i < batch->drawCounter; i++)
            {
                rlDrawCall *draw = &batch->draws[i];

                BindTexture(draw->textureId);

                if (draw->mode == RL_LINES)
                {
                    BindPipeline(dxState.defaultLineShaderId);
                    driver.commandList->lpVtbl->IASetPrimitiveTopology(driver.commandList, D3D_PRIMITIVE_TOPOLOGY_LINELIST);
                    driver.commandList->lpVtbl->DrawInstanced(driver.commandList, draw->vertexCount, 1, vertexOffset, 0);
                }
                else
                {
                    BindPipeline(dxState.defaultShaderId);
                    driver.commandList->lpVtbl->IASetPrimitiveTopology(driver.commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    if (draw->mode == RL_TRIANGLES)
                    {
                        driver.commandList->lpVtbl->DrawInstanced(driver.commandList, draw->vertexCount, 1, vertexOffset, 0);
                    }
                    else
                    {
                        const int indexCount = draw->vertexCount / 4 * 6;
                        driver.commandList->lpVtbl->DrawIndexedInstanced(driver.commandList, indexCount, 1, 0, vertexOffset, 0);
                    }
                }

                vertexOffset += draw->vertexCount + draw->vertexAlignment;
            }
        }
    }


    dxState.vertexCounter = 0;
    batch->currentDepth = -1.0f;

    for (int i = 0; i < RL_DEFAULT_BATCH_DRAWCALLS; i++)
    {
        batch->draws[i].mode = RL_QUADS;
        batch->draws[i].vertexCount = 0;
        batch->draws[i].textureId = dxState.defaultTextureId;
    }

    batch->drawCounter = 1;
    batch->currentBuffer++;
    
    if (batch->currentBuffer >= batch->bufferCount)
    {
        batch->currentBuffer = 0;
    }

    ExecuteCommands();
    WaitForPreviousFrame();
    ResetCommands();
}

void rlSetRenderBatchActive(rlRenderBatch *batch)
{
    rlDrawRenderBatch(dxState.currentBatch);

    if (batch != NULL)
    {
        dxState.currentBatch = batch;
    }
    else
    {
        dxState.currentBatch = &dxState.defaultBatch;
    }
}

void rlDrawRenderBatchActive(void)
{
    rlDrawRenderBatch(dxState.currentBatch);
}

bool rlCheckRenderBatchLimit(int vCount)
{
    bool overflow = false;

    if ((dxState.vertexCounter + vCount) >=
        (dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].elementCount*4))
    {
        overflow = true;

        // Store current primitive drawing mode and texture id
        int currentMode = dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode;
        int currentTexture = dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].textureId;

        rlDrawRenderBatch(dxState.currentBatch);    // NOTE: Stereo rendering is checked inside

        // Restore state of last batch so we can continue adding vertices
        dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode = currentMode;
        dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].textureId = currentTexture;
    }

    return overflow;
}

void rlSetTexture(unsigned int id)
{
    if (id == 0)
    {
        // NOTE: If quads batch limit is reached, we force a draw call and next batch starts
        if (dxState.vertexCounter >=
            dxState.currentBatch->vertexBuffer[dxState.currentBatch->currentBuffer].elementCount*4)
        {
            rlDrawRenderBatch(dxState.currentBatch);
        }
    }
    else
    {
        if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].textureId != id)
        {
            if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount > 0)
            {
                if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode == RL_LINES) dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment = ((dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount < 4)? dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount : dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount%4);
                else if (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].mode == RL_TRIANGLES) dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment = ((dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount < 4)? 1 : (4 - (dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount%4)));
                else dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment = 0;

                if (!rlCheckRenderBatchLimit(dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment))
                {
                    dxState.vertexCounter += dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexAlignment;

                    dxState.currentBatch->drawCounter++;
                }
            }

            if (dxState.currentBatch->drawCounter >= RL_DEFAULT_BATCH_DRAWCALLS) rlDrawRenderBatch(dxState.currentBatch);

            dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].textureId = id;
            dxState.currentBatch->draws[dxState.currentBatch->drawCounter - 1].vertexCount = 0;
        }
    }
}

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

typedef struct {
    void *data;
    bool needsFree;
    int format;
} TransformedData;

static TransformedData TransformData(const void *data, int width, int height, int format)
{
    TransformedData result = { 0 };
    result.data = (void*)data;
    result.needsFree = false;
    result.format = format;

    if (format == RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8)
    {
        unsigned char *src = (char*)data;
        unsigned char *dst = (char*)RL_MALLOC(width * height * 4 * sizeof(char));

        int srcOffset = 0;
        int dstOffset = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                dst[dstOffset + 0] = src[srcOffset + 0];
                dst[dstOffset + 1] = src[srcOffset + 1];
                dst[dstOffset + 2] = src[srcOffset + 2];
                dst[dstOffset + 3] = 255;

                srcOffset += 3;
                dstOffset += 4;
            }
        }

        result.data = dst;
        result.needsFree = true;
        result.format = RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    }

    return result;
}

// Textures management
unsigned int rlLoadTexture(const void *data, int width, int height, int format, int mipmapCount)
{
    DXTexture texture = { 0 };

    TransformedData transformedData = TransformData(data, width, height, format);

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
    description.Format = ToDXGIFormat(transformedData.format);
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    description.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT result = driver.device->lpVtbl->CreateCommittedResource(driver.device, &heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (LPVOID*)&texture.data);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to create texture resource!");
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
        DXTRACELOG(RL_LOG_ERROR, "Failed to create texture upload resource!");
        return 0;
    }

    unsigned char *uploadBuffer = NULL;
    result = texture.upload->lpVtbl->Map(texture.upload, 0, NULL, (LPVOID*)&uploadBuffer);
    if (FAILED(result))
    {
        DXRELEASE(texture.data);
        DXRELEASE(texture.upload);
        DXTRACELOG(RL_LOG_ERROR, "Failed to map upload resource memory!");
        return 0;
    }

    D3D12_MEMCPY_DEST memcpyDest = { 0 };
    memcpyDest.pData = uploadBuffer + layouts.Offset;
    memcpyDest.RowPitch = layouts.Footprint.RowPitch;
    memcpyDest.SlicePitch = (UINT64)layouts.Footprint.RowPitch * (UINT64)numRows;

    D3D12_SUBRESOURCE_DATA textureData = { 0 };
    textureData.pData = transformedData.data;
    textureData.RowPitch = width * StrideInBytes(transformedData.format);
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
    texture.offset = index;
    texture.width = width;
    texture.height = height;

    VectorPush(&driver.textures.pool, &texture);

    if (transformedData.needsFree)
    {
        RL_FREE(transformedData.data);
    }

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
unsigned int rlLoadShaderCode(const char *vsCode, const char *fsCode)
{
    const unsigned int vShaderId = rlCompileShader(vsCode, RL_VERTEX_SHADER);
    if (vShaderId == 0)
    {
        return 0;
    }

    const unsigned int fShaderId = rlCompileShader(fsCode, RL_FRAGMENT_SHADER);
    if (fShaderId == 0)
    {
        return 0;
    }

    return rlLoadShaderProgram(vShaderId, fShaderId);
}

unsigned int rlCompileShader(const char *shaderCode, int type)
{
    const size_t len = strlen(shaderCode);

    UINT compileFlags = 0;

#if defined(DIRECTX_SHADER_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    DXShader shader;
    shader.type = type;

    ID3DBlob *errors = NULL;
    const char *sourceName = type == RL_VERTEX_SHADER ? "vertex" : "fragment";
    const char *target = type == RL_VERTEX_SHADER ? "vs_5_0" : "ps_5_0";
    HRESULT result = D3DCompile(shaderCode, len, sourceName, NULL, NULL, "Main", target, compileFlags, 0, &shader.data, &errors);
    if (FAILED(result))
    {
        DXTRACELOG(RL_LOG_ERROR, "Failed to compile %s shader! Error: %s", sourceName, (LPCSTR)errors->lpVtbl->GetBufferPointer(errors));
        return 0;
    }

    shader.id = driver.shaders.index++;
    VectorPush(&driver.shaders.pool, &shader);

    return shader.id;
}

unsigned int rlLoadShaderProgram(unsigned int vShaderId, unsigned int fShaderId)
{
    return CreatePipeline(vShaderId, fShaderId, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
}

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
Matrix rlGetMatrixModelview(void)
{
    return dxState.matrices.modelView;
}

Matrix rlGetMatrixProjection(void)
{
    return dxState.matrices.projection;
}

Matrix rlGetMatrixTransform(void)
{
    return dxState.matrices.transform;
}

Matrix rlGetMatrixProjectionStereo(int eye) { Matrix result = { 0 }; return result; }
Matrix rlGetMatrixViewOffsetStereo(int eye) { Matrix result = { 0 }; return result; }

void rlSetMatrixProjection(Matrix proj)
{
    dxState.matrices.projection = proj;
}

void rlSetMatrixModelview(Matrix view)
{
    dxState.matrices.modelView = view;
}

void rlSetMatrixProjectionStereo(Matrix right, Matrix left) {}
void rlSetMatrixViewOffsetStereo(Matrix right, Matrix left) {}

// Quick and dirty cube/quad buffers load->draw->unload
void rlLoadDrawCube(void) {}
void rlLoadDrawQuad(void) {}

void rlPresent()
{
    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.pResource = driver.renderTargets[driver.frameIndex];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    driver.commandList->lpVtbl->ResourceBarrier(driver.commandList, 1, &barrier);

    ExecuteCommands();

    if (FAILED(driver.swapChain->lpVtbl->Present(driver.swapChain, 1, 0)))
    {
        DXTRACELOG(RL_LOG_WARNING, "Failed to present!");
    }

    WaitForPreviousFrame();
    ResetCommands();

    driver.frameIndex = driver.swapChain->lpVtbl->GetCurrentBackBufferIndex(driver.swapChain);

#if defined(DIRECTX_INFOQUEUE)
    PollInfoQueue();
#endif

    // Prepare render target for next frame
    UpdateRenderTarget();
}
