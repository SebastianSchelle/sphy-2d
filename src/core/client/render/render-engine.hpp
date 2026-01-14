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

namespace gfx
{

#define INVALID_GEOMETRY_HANDLE 0

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
    glm::ivec2 getTextureSize() const;
    ShaderHandle loadShader(const std::string& name,
                            const std::string& vsPath,
                            const std::string& fsPath);
    void releaseShader(ShaderHandle handle);
    void drawRectangle(const glm::vec2& translation,
                       float rotation,
                       TextureHandle textureHandle,
                       bgfx::ViewId viewId = 0);
    void drawFullScreenTriangles(bgfx::ViewId viewId, ShaderHandle shaderHandle);
    ShaderHandle getShaderHandle(const std::string& name);

  private:
    void cleanUpAll();
    void updateOrtho();
    void cleanUpTextures();
    void cleanUpShaders();
    void cleanUpGeometry();
    void changeRenderState(RenderState newState);

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

    ShaderHandle shaderHandleRml = ShaderHandle::Invalid();
    TextureHandle textureHandleFallback = TextureHandle::Invalid();

    tim::Timepoint startTime;
    float frameTime;

    int winWidth;
    int winHeight;
    float ortho[16];
    glm::vec2 scissorRegionPosition;
    glm::vec2 scissorRegionSize;
    bool scissorRegionEnabled;
    int texWidth;
    int texHeight;
    RenderState renderState;
    const bgfx::ViewId kClearView = 0;
};

}  // namespace gfx

#endif