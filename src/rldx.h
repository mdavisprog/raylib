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

#ifndef RLDX_H
#define RLDX_H

#ifndef RLAPI
    #define RLAPI
#endif

#define RL_TEXTURE_MAG_FILTER 0
#define RL_TEXTURE_MIN_FILTER 0
#define RL_TEXTURE_FILTER_LINEAR 0
#define RL_MODELVIEW 0
#define RL_PROJECTION 0
#define RL_OPENGL_11 1

#if (defined(__STDC__) && __STDC_VERSION__ >= 199901L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
    #include <stdbool.h>
#elif !defined(__cplusplus) && !defined(bool) && !defined(RL_BOOL_TYPE)
    // Boolean type
typedef enum bool { false = 0, true = !false } bool;
#endif

#if !defined(RL_MATRIX_TYPE)
// Matrix, 4x4 components, column major, OpenGL style, right handed
typedef struct Matrix {
    float m0, m4, m8, m12;      // Matrix first row (4 components)
    float m1, m5, m9, m13;      // Matrix second row (4 components)
    float m2, m6, m10, m14;     // Matrix third row (4 components)
    float m3, m7, m11, m15;     // Matrix fourth row (4 components)
} Matrix;
#define RL_MATRIX_TYPE
#endif

typedef struct rlRenderBatch {
    int dummy;
} rlRenderBatch;

RLAPI void rlMatrixMode(int mode);                      // Choose the current matrix to be transformed
RLAPI void rlPushMatrix(void);                          // Push the current matrix to stack
RLAPI void rlPopMatrix(void);                           // Pop latest inserted matrix from stack
RLAPI void rlLoadIdentity(void);                        // Reset current matrix to identity matrix
RLAPI void rlTranslatef(float x, float y, float z);     // Multiply the current matrix by a translation matrix
RLAPI void rlRotatef(float angle, float x, float y, float z); // Multiply the current matrix by a rotation matrix
RLAPI void rlScalef(float x, float y, float z);         // Multiply the current matrix by a scaling matrix
RLAPI void rlMultMatrixf(const float *matf);            // Multiply the current matrix by another matrix
RLAPI void rlFrustum(double left, double right, double bottom, double top, double znear, double zfar);
RLAPI void rlOrtho(double left, double right, double bottom, double top, double znear, double zfar);
RLAPI void rlViewport(int x, int y, int width, int height); // Set the viewport area
RLAPI void rlSetClipPlanes(double nearPlane, double farPlane);    // Set clip planes distances
RLAPI double rlGetCullDistanceNear(void);               // Get cull plane distance near
RLAPI double rlGetCullDistanceFar(void);                // Get cull plane distance far

//------------------------------------------------------------------------------------
// Functions Declaration - Vertex level operations
//------------------------------------------------------------------------------------
RLAPI void rlBegin(int mode);                           // Initialize drawing mode (how to organize vertex)
RLAPI void rlEnd(void);                                 // Finish vertex providing
RLAPI void rlVertex2i(int x, int y);                    // Define one vertex (position) - 2 int
RLAPI void rlVertex2f(float x, float y);                // Define one vertex (position) - 2 float
RLAPI void rlVertex3f(float x, float y, float z);       // Define one vertex (position) - 3 float
RLAPI void rlTexCoord2f(float x, float y);              // Define one vertex (texture coordinate) - 2 float
RLAPI void rlNormal3f(float x, float y, float z);       // Define one vertex (normal) - 3 float
RLAPI void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a); // Define one vertex (color) - 4 byte
RLAPI void rlColor3f(float x, float y, float z);        // Define one vertex (color) - 3 float
RLAPI void rlColor4f(float x, float y, float z, float w); // Define one vertex (color) - 4 float

//------------------------------------------------------------------------------------
// Functions Declaration - OpenGL style functions (common to 1.1, 3.3+, ES2)
// NOTE: This functions are used to completely abstract raylib code from OpenGL layer,
// some of them are direct wrappers over OpenGL calls, some others are custom
//------------------------------------------------------------------------------------

// Vertex buffers state
RLAPI bool rlEnableVertexArray(unsigned int vaoId);     // Enable vertex array (VAO, if supported)
RLAPI void rlDisableVertexArray(void);                  // Disable vertex array (VAO, if supported)
RLAPI void rlEnableVertexBuffer(unsigned int id);       // Enable vertex buffer (VBO)
RLAPI void rlDisableVertexBuffer(void);                 // Disable vertex buffer (VBO)
RLAPI void rlEnableVertexBufferElement(unsigned int id); // Enable vertex buffer element (VBO element)
RLAPI void rlDisableVertexBufferElement(void);          // Disable vertex buffer element (VBO element)
RLAPI void rlEnableVertexAttribute(unsigned int index); // Enable vertex attribute index
RLAPI void rlDisableVertexAttribute(unsigned int index); // Disable vertex attribute index

// Textures state
RLAPI void rlActiveTextureSlot(int slot);               // Select and active a texture slot
RLAPI void rlEnableTexture(unsigned int id);            // Enable texture
RLAPI void rlDisableTexture(void);                      // Disable texture
RLAPI void rlEnableTextureCubemap(unsigned int id);     // Enable texture cubemap
RLAPI void rlDisableTextureCubemap(void);               // Disable texture cubemap
RLAPI void rlTextureParameters(unsigned int id, int param, int value); // Set texture parameters (filter, wrap)
RLAPI void rlCubemapParameters(unsigned int id, int param, int value); // Set cubemap parameters (filter, wrap)

// Shader state
RLAPI void rlEnableShader(unsigned int id);             // Enable shader program
RLAPI void rlDisableShader(void);                       // Disable shader program

// Framebuffer state
RLAPI void rlEnableFramebuffer(unsigned int id);        // Enable render texture (fbo)
RLAPI void rlDisableFramebuffer(void);                  // Disable render texture (fbo), return to default framebuffer
RLAPI unsigned int rlGetActiveFramebuffer(void);        // Get the currently active render texture (fbo), 0 for default framebuffer
RLAPI void rlActiveDrawBuffers(int count);              // Activate multiple draw color buffers
RLAPI void rlBlitFramebuffer(int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight, int bufferMask); // Blit active framebuffer to main framebuffer
RLAPI void rlBindFramebuffer(unsigned int target, unsigned int framebuffer); // Bind framebuffer (FBO)

// General render state
RLAPI void rlEnableColorBlend(void);                    // Enable color blending
RLAPI void rlDisableColorBlend(void);                   // Disable color blending
RLAPI void rlEnableDepthTest(void);                     // Enable depth test
RLAPI void rlDisableDepthTest(void);                    // Disable depth test
RLAPI void rlEnableDepthMask(void);                     // Enable depth write
RLAPI void rlDisableDepthMask(void);                    // Disable depth write
RLAPI void rlEnableBackfaceCulling(void);               // Enable backface culling
RLAPI void rlDisableBackfaceCulling(void);              // Disable backface culling
RLAPI void rlColorMask(bool r, bool g, bool b, bool a); // Color mask control
RLAPI void rlSetCullFace(int mode);                     // Set face culling mode
RLAPI void rlEnableScissorTest(void);                   // Enable scissor test
RLAPI void rlDisableScissorTest(void);                  // Disable scissor test
RLAPI void rlScissor(int x, int y, int width, int height); // Scissor test
RLAPI void rlEnableWireMode(void);                      // Enable wire mode
RLAPI void rlEnablePointMode(void);                     // Enable point mode
RLAPI void rlDisableWireMode(void);                     // Disable wire mode ( and point ) maybe rename
RLAPI void rlSetLineWidth(float width);                 // Set the line drawing width
RLAPI float rlGetLineWidth(void);                       // Get the line drawing width
RLAPI void rlEnableSmoothLines(void);                   // Enable line aliasing
RLAPI void rlDisableSmoothLines(void);                  // Disable line aliasing
RLAPI void rlEnableStereoRender(void);                  // Enable stereo rendering
RLAPI void rlDisableStereoRender(void);                 // Disable stereo rendering
RLAPI bool rlIsStereoRenderEnabled(void);               // Check if stereo render is enabled

RLAPI void rlClearColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a); // Clear color buffer with color
RLAPI void rlClearScreenBuffers(void);                  // Clear used screen buffers (color and depth)
RLAPI void rlCheckErrors(void);                         // Check and log OpenGL error codes
RLAPI void rlSetBlendMode(int mode);                    // Set blending mode
RLAPI void rlSetBlendFactors(int glSrcFactor, int glDstFactor, int glEquation); // Set blending mode factor and equation (using OpenGL factors)
RLAPI void rlSetBlendFactorsSeparate(int glSrcRGB, int glDstRGB, int glSrcAlpha, int glDstAlpha, int glEqRGB, int glEqAlpha); // Set blending mode factors and equations separately (using OpenGL factors)

//------------------------------------------------------------------------------------
// Functions Declaration - rlgl functionality
//------------------------------------------------------------------------------------
// rlgl initialization functions
RLAPI void rlglInit(int width, int height);             // Initialize rlgl (buffers, shaders, textures, states)
RLAPI void rlglClose(void);                             // De-initialize rlgl (buffers, shaders, textures)
RLAPI void rlLoadExtensions(void *loader);              // Load OpenGL extensions (loader function required)
RLAPI int rlGetVersion(void);                           // Get current OpenGL version
RLAPI void rlSetFramebufferWidth(int width);            // Set current framebuffer width
RLAPI int rlGetFramebufferWidth(void);                  // Get default framebuffer width
RLAPI void rlSetFramebufferHeight(int height);          // Set current framebuffer height
RLAPI int rlGetFramebufferHeight(void);                 // Get default framebuffer height

RLAPI unsigned int rlGetTextureIdDefault(void);         // Get default texture id
RLAPI unsigned int rlGetShaderIdDefault(void);          // Get default shader id
RLAPI int *rlGetShaderLocsDefault(void);                // Get default shader locations

// Render batch management
// NOTE: rlgl provides a default render batch to behave like OpenGL 1.1 immediate mode
// but this render batch API is exposed in case of custom batches are required
RLAPI rlRenderBatch rlLoadRenderBatch(int numBuffers, int bufferElements); // Load a render batch system
RLAPI void rlUnloadRenderBatch(rlRenderBatch batch);    // Unload render batch system
RLAPI void rlDrawRenderBatch(rlRenderBatch *batch);     // Draw render batch data (Update->Draw->Reset)
RLAPI void rlSetRenderBatchActive(rlRenderBatch *batch); // Set the active render batch for rlgl (NULL for default internal)
RLAPI void rlDrawRenderBatchActive(void);               // Update and draw internal render batch
RLAPI bool rlCheckRenderBatchLimit(int vCount);         // Check internal buffer overflow for a given number of vertex

RLAPI void rlSetTexture(unsigned int id);               // Set current texture for render batch and check buffers limits

//------------------------------------------------------------------------------------------------------------------------

// Vertex buffers management
RLAPI unsigned int rlLoadVertexArray(void);             // Load vertex array (vao) if supported
RLAPI unsigned int rlLoadVertexBuffer(const void *buffer, int size, bool dynamic); // Load a vertex buffer object
RLAPI unsigned int rlLoadVertexBufferElement(const void *buffer, int size, bool dynamic); // Load vertex buffer elements object
RLAPI void rlUpdateVertexBuffer(unsigned int bufferId, const void *data, int dataSize, int offset); // Update vertex buffer object data on GPU buffer
RLAPI void rlUpdateVertexBufferElements(unsigned int id, const void *data, int dataSize, int offset); // Update vertex buffer elements data on GPU buffer
RLAPI void rlUnloadVertexArray(unsigned int vaoId);     // Unload vertex array (vao)
RLAPI void rlUnloadVertexBuffer(unsigned int vboId);    // Unload vertex buffer object
RLAPI void rlSetVertexAttribute(unsigned int index, int compSize, int type, bool normalized, int stride, int offset); // Set vertex attribute data configuration
RLAPI void rlSetVertexAttributeDivisor(unsigned int index, int divisor); // Set vertex attribute data divisor
RLAPI void rlSetVertexAttributeDefault(int locIndex, const void *value, int attribType, int count); // Set vertex attribute default value, when attribute to provided
RLAPI void rlDrawVertexArray(int offset, int count);    // Draw vertex array (currently active vao)
RLAPI void rlDrawVertexArrayElements(int offset, int count, const void *buffer); // Draw vertex array elements
RLAPI void rlDrawVertexArrayInstanced(int offset, int count, int instances); // Draw vertex array (currently active vao) with instancing
RLAPI void rlDrawVertexArrayElementsInstanced(int offset, int count, const void *buffer, int instances); // Draw vertex array elements with instancing

// Textures management
RLAPI unsigned int rlLoadTexture(const void *data, int width, int height, int format, int mipmapCount); // Load texture data
RLAPI unsigned int rlLoadTextureDepth(int width, int height, bool useRenderBuffer); // Load depth texture/renderbuffer (to be attached to fbo)
RLAPI unsigned int rlLoadTextureCubemap(const void *data, int size, int format); // Load texture cubemap data
RLAPI void rlUpdateTexture(unsigned int id, int offsetX, int offsetY, int width, int height, int format, const void *data); // Update texture with new data on GPU
RLAPI void rlGetGlTextureFormats(int format, unsigned int *glInternalFormat, unsigned int *glFormat, unsigned int *glType); // Get OpenGL internal formats
RLAPI const char *rlGetPixelFormatName(unsigned int format);              // Get name string for pixel format
RLAPI void rlUnloadTexture(unsigned int id);                              // Unload texture from GPU memory
RLAPI void rlGenTextureMipmaps(unsigned int id, int width, int height, int format, int *mipmaps); // Generate mipmap data for selected texture
RLAPI void *rlReadTexturePixels(unsigned int id, int width, int height, int format); // Read texture pixel data
RLAPI unsigned char *rlReadScreenPixels(int width, int height);           // Read screen pixel data (color buffer)

// Framebuffer management (fbo)
RLAPI unsigned int rlLoadFramebuffer(void);                               // Load an empty framebuffer
RLAPI void rlFramebufferAttach(unsigned int fboId, unsigned int texId, int attachType, int texType, int mipLevel); // Attach texture/renderbuffer to a framebuffer
RLAPI bool rlFramebufferComplete(unsigned int id);                        // Verify framebuffer is complete
RLAPI void rlUnloadFramebuffer(unsigned int id);                          // Delete framebuffer from GPU

// Shaders management
RLAPI unsigned int rlLoadShaderCode(const char *vsCode, const char *fsCode);    // Load shader from code strings
RLAPI unsigned int rlCompileShader(const char *shaderCode, int type);           // Compile custom shader and return shader id (type: RL_VERTEX_SHADER, RL_FRAGMENT_SHADER, RL_COMPUTE_SHADER)
RLAPI unsigned int rlLoadShaderProgram(unsigned int vShaderId, unsigned int fShaderId); // Load custom shader program
RLAPI void rlUnloadShaderProgram(unsigned int id);                              // Unload shader program
RLAPI int rlGetLocationUniform(unsigned int shaderId, const char *uniformName); // Get shader location uniform
RLAPI int rlGetLocationAttrib(unsigned int shaderId, const char *attribName);   // Get shader location attribute
RLAPI void rlSetUniform(int locIndex, const void *value, int uniformType, int count); // Set shader value uniform
RLAPI void rlSetUniformMatrix(int locIndex, Matrix mat);                        // Set shader value matrix
RLAPI void rlSetUniformSampler(int locIndex, unsigned int textureId);           // Set shader value sampler
RLAPI void rlSetShader(unsigned int id, int *locs);                             // Set shader currently active (id and locations)

// Compute shader management
RLAPI unsigned int rlLoadComputeShaderProgram(unsigned int shaderId);           // Load compute shader program
RLAPI void rlComputeShaderDispatch(unsigned int groupX, unsigned int groupY, unsigned int groupZ); // Dispatch compute shader (equivalent to *draw* for graphics pipeline)

// Shader buffer storage object management (ssbo)
RLAPI unsigned int rlLoadShaderBuffer(unsigned int size, const void *data, int usageHint); // Load shader storage buffer object (SSBO)
RLAPI void rlUnloadShaderBuffer(unsigned int ssboId);                           // Unload shader storage buffer object (SSBO)
RLAPI void rlUpdateShaderBuffer(unsigned int id, const void *data, unsigned int dataSize, unsigned int offset); // Update SSBO buffer data
RLAPI void rlBindShaderBuffer(unsigned int id, unsigned int index);             // Bind SSBO buffer
RLAPI void rlReadShaderBuffer(unsigned int id, void *dest, unsigned int count, unsigned int offset); // Read SSBO buffer data (GPU->CPU)
RLAPI void rlCopyShaderBuffer(unsigned int destId, unsigned int srcId, unsigned int destOffset, unsigned int srcOffset, unsigned int count); // Copy SSBO data between buffers
RLAPI unsigned int rlGetShaderBufferSize(unsigned int id);                      // Get SSBO buffer size

// Buffer management
RLAPI void rlBindImageTexture(unsigned int id, unsigned int index, int format, bool readonly);  // Bind image texture

// Matrix state management
RLAPI Matrix rlGetMatrixModelview(void);                                  // Get internal modelview matrix
RLAPI Matrix rlGetMatrixProjection(void);                                 // Get internal projection matrix
RLAPI Matrix rlGetMatrixTransform(void);                                  // Get internal accumulated transform matrix
RLAPI Matrix rlGetMatrixProjectionStereo(int eye);                        // Get internal projection matrix for stereo render (selected eye)
RLAPI Matrix rlGetMatrixViewOffsetStereo(int eye);                        // Get internal view offset matrix for stereo render (selected eye)
RLAPI void rlSetMatrixProjection(Matrix proj);                            // Set a custom projection matrix (replaces internal projection matrix)
RLAPI void rlSetMatrixModelview(Matrix view);                             // Set a custom modelview matrix (replaces internal modelview matrix)
RLAPI void rlSetMatrixProjectionStereo(Matrix right, Matrix left);        // Set eyes projection matrices for stereo rendering
RLAPI void rlSetMatrixViewOffsetStereo(Matrix right, Matrix left);        // Set eyes view offsets matrices for stereo rendering

// Quick and dirty cube/quad buffers load->draw->unload
RLAPI void rlLoadDrawCube(void);     // Load and draw a cube
RLAPI void rlLoadDrawQuad(void);     // Load and draw a quad

// Used from SwapScreenBuffers
void rlPresent();

#endif // RLDX_H
