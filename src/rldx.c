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

#include <stdio.h>

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


void rlglInit(int width, int height) {}
void rlglClose(void) {}
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
