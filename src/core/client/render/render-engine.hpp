#ifndef RENDER_ENGINE_HPP
#define RENDER_ENGINE_HPP

#include "std-inc.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <config-manager.hpp>
#include <item-lib.hpp>
#include <shader.hpp>
#include <texture.hpp>
#include <vertex-defines.hpp>

namespace gfx
{

#define INVALID_GEOMETRY_HANDLE 0
#define MAX_SHAPES 1024
#define MAX_SHAPE_VERTICES MAX_SHAPES * 4
#define MAX_SHAPE_INDICES MAX_SHAPES * 6

#define SHAPE_TYPE_RECTANGLE 1.0f
#define SHAPE_TYPE_CIRCLE 2.0f
#define SHAPE_TYPE_TRIANGLE 3.0f
#define SHAPE_TYPE_LINE 4.0f

struct Geometry
{
  public:
    Geometry(const void* vertexData,
             size_t vDatSize,
             const void* indexData,
             size_t iDatSize,
             bgfx::VertexLayout& vertLayout,
             bool use32BitIndices = false);

    bgfx::VertexBufferHandle getVbh() const;
    bgfx::IndexBufferHandle getIbh() const;
    void destroy() const;

  private:
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
};

// Type alias for Geometry handle - must be after Geometry is fully defined
using GeometryHandle = typename con::ItemLib<Geometry>::Handle;
using GeometryHandleUuid = typename con::ItemLib<Geometry>::HandleUuid;

class RenderEngine
{
  public:
    enum class RenderState
    {
        Idle,
        DrawTexRects,
        DrawFullScreenTriangles,
        DrawCompiledGeometry,
        DrawShapes,
    };

    RenderEngine(cfg::ConfigManager& config);
    ~RenderEngine();

    // Prevent copying and moving (contains BGFX handles and other resources)
    RenderEngine(const RenderEngine&) = delete;
    RenderEngine& operator=(const RenderEngine&) = delete;
    RenderEngine(RenderEngine&&) = delete;
    RenderEngine& operator=(RenderEngine&&) = delete;

    bool initPre();
    bool initPost();

    void startFrame();
    void endFrame();

    GeometryHandle compileGeometry(const void* vertexData,
                                   size_t vDatSize,
                                   const void* indexData,
                                   size_t iDatSize,
                                   bgfx::VertexLayout& vertLayout,
                                   bool use32BitIndices = false);
    void releaseGeometry(GeometryHandle handle);
    void renderCompiledGeometry(GeometryHandle handle,
                                const glm::vec2& translation,
                                TextureHandle textureHandle,
                                bgfx::ViewId viewId = 0);
    void setWindowSize(int width, int height);
    void setWorldCamera(float cameraX, float cameraY, float zoom);
    TextureHandle loadTexture(const std::string& name,
                              const std::string& type,
                              const std::string& path);
    TextureHandle loadTexture(const std::string& name,
                              const std::string& type,
                              const std::string& path,
                              glm::vec2& dimensions);
    TextureHandle generateTexture(const std::string& name,
                                  const std::string& type,
                                  const void* data,
                                  int width,
                                  int height,
                                  const std::string& path = "");
    void setScissorRegion(const glm::vec2& position, const glm::vec2& size);
    void enableScissorRegion(bool enable);
    void setTransform(const glm::mat4& transform);
    glm::ivec2 getTextureSize() const;
    ShaderHandle loadShader(const std::string& name,
                            const std::string& vsPath,
                            const std::string& fsPath);
    void releaseShader(ShaderHandle handle);
    void drawRectangle(const glm::vec2& translation,
                       float rotation,
                       TextureHandle textureHandle,
                       bgfx::ViewId viewId = 0);
    void drawFullScreenTriangles(bgfx::ViewId viewId,
                                 ShaderHandle shaderHandle);
    ShaderHandle getShaderHandle(const std::string& name);
    void drawRectangle(const glm::vec2& pos,
                       const glm::vec2& size,
                       uint32_t colorRGBA,
                       float thickness,
                       float rotationRad = 0.0f,
                       bgfx::ViewId viewId = 0);
    void drawEllipse(const glm::vec2& pos,
                    const glm::vec2& size,
                    uint32_t colorRGBA,
                    float thickness,
                    float rotationRad = 0.0f,
                    bgfx::ViewId viewId = 0);
    tim::Timepoint getStartTime() const;

  private:
    void cleanUpAll();
    void updateOrtho();
    void updateWorldView();
    void cleanUpTextures();
    void cleanUpShaders();
    void cleanUpGeometry();
    void changeRenderState(RenderState newState);
    void submitShapes();
    void allocateForShapes();
    void drawBoxShape(float shapeType,
                      const glm::vec2& pos,
                      const glm::vec2& size,
                      uint32_t colorRGBA,
                      float thickness,
                      float rotationRad = 0.0f,
                      bgfx::ViewId viewId = 0);


    TextureLoader textureLoader;
    cfg::ConfigManager& config;

    con::ItemLib<Geometry> compiledGeometryLib;
    con::ItemLib<ShaderProgram> compiledShaderLib;

    bgfx::VertexBufferHandle vbhRectangle;
    bgfx::IndexBufferHandle ibhRectangle;
    bgfx::VertexBufferHandle vbhFullScreenTriangles;
    bgfx::IndexBufferHandle ibhFullScreenTriangles;

    bgfx::UniformHandle u_translation = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texArray = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texLayer = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_atlasPos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_time = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_transform = BGFX_INVALID_HANDLE;

    ShaderHandle shaderHandleRml = ShaderHandle::Invalid();
    ShaderHandle shaderHandleShapes = ShaderHandle::Invalid();
    TextureHandle textureHandleFallback = TextureHandle::Invalid();

    tim::Timepoint startTime;
    float frameTime;

    int winWidth;
    int winHeight;
    float ortho[16];
    float worldCameraX = 0.0f;
    float worldCameraY = 0.0f;
    float worldZoom = 2.0f;
    float worldView[16];
    float worldViewProj[16];
    glm::vec2 scissorRegionPosition;
    glm::vec2 scissorRegionSize;
    bool scissorRegionEnabled;
    int texWidth;
    int texHeight;
    RenderState renderState;
    const bgfx::ViewId kWorldView = 0;
    const bgfx::ViewId kUiView = 1;
    glm::mat4 geomTransformMatrix;

    bgfx::TransientVertexBuffer tvbSdf;
    bgfx::TransientIndexBuffer tibSdf;
    bgfx::ViewId currentViewId = kWorldView;
    uint32_t currentShapeCount = 0;
    uint32_t currentShapeVertices = 0;
    uint32_t currentShapeIndices = 0;
};

}  // namespace gfx

#endif