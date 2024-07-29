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
#include <dxgi1_6.h>
#include <stdio.h>

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------

#define DXRELEASE(object) if (object != NULL) { object->lpVtbl->Release(object); object = NULL; }

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------

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
    DescriptorHeap SRV;
    DescriptorHeap RTV;
    IDXGISwapChain4 *swapChain;
    ID3D12RootSignature *rootSignature;
    ID3D12Fence *fence;
    UINT64 fenceValue;
    HANDLE fenceEvent;
    UINT frameIndex;
    ID3D12Resource *renderTargets[2];
} DriverData;

//----------------------------------------------------------------------------------
// Variables
//----------------------------------------------------------------------------------

static DriverData driver = { 0 };

//----------------------------------------------------------------------------------
// External functions
//----------------------------------------------------------------------------------

extern void *GetWindowHandle(void);

//----------------------------------------------------------------------------------
// Utility functions
//----------------------------------------------------------------------------------

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
    IDXGIFactory7* factory = NULL;
    HRESULT result = CreateDXGIFactory2(0, &IID_IDXGIFactory7, (LPVOID*)&factory);
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

static D3D12_CPU_DESCRIPTOR_HANDLE CPUOffset(DescriptorHeap* heap, UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE result = { 0 };
    heap->descriptorHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap->descriptorHeap, &result);
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

    D3D12_CPU_DESCRIPTOR_HANDLE offset = CPUOffset(&driver.RTV, index);
    driver.device->lpVtbl->CreateRenderTargetView(driver.device, driver.renderTargets[index], NULL, offset);

    return TRUE;
}

static bool InitializeSwapChain(UINT width, UINT height)
{
    if (!CreateDescriptorHeap(&driver.RTV, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_NONE))
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

void rlClearColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {}
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

    if (!CreateDescriptorHeap(&driver.SRV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE))
    {
        printf("DIRECTX: Failed to create SRV descriptor heap!\n");
        return;
    }

    if (!InitializeRootSignature())
    {
        return;
    }

    if (!InitializeFence())
    {
        return;
    }

    if (!InitializeSwapChain(width, height))
    {
        return;
    }

    DXGI_ADAPTER_DESC1 desc = { 0 };
    if (FAILED(driver.adapter->lpVtbl->GetDesc1(driver.adapter, &desc)))
    {
        printf("DIRECTX: Failed to retrieve adapter description!\n");
        return;
    }

    printf("DIRECTX: Initialized DirectX!\n");

    char* driverName = Windows_ToMultiByte(desc.Description);
    printf("DIRECTX: Driver is %s.\n", driverName);
    free(driverName);
}

void rlglClose(void)
{
    DXRELEASE(driver.renderTargets[0]);
    DXRELEASE(driver.renderTargets[1]);
    DXRELEASE(driver.swapChain);
    DXRELEASE(driver.fence);
    DXRELEASE(driver.rootSignature);
    DXRELEASE(driver.RTV.descriptorHeap);
    DXRELEASE(driver.SRV.descriptorHeap);
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
void rlDrawRenderBatchActive(void) {}
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
unsigned int rlLoadTexture(const void *data, int width, int height, int format, int mipmapCount) { return 0; }
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

    WaitForPreviousFrame();
    driver.frameIndex = driver.swapChain->lpVtbl->GetCurrentBackBufferIndex(driver.swapChain);
}
