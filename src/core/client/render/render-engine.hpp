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
    void destroy();

  private:
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
};

class RenderEngine
{
  public:
    RenderEngine(cfg::ConfigManager& config);
    ~RenderEngine();

    void init();
    uint32_t compileGeometry(const void* vertexData,
                             size_t vDatSize,
                             const void* indexData,
                             size_t iDatSize,
                             bgfx::VertexLayout& vertLayout,
                             bool use32BitIndices = false);
    void releaseGeometry(uint32_t handle);
    void renderCompiledGeometry(uint32_t handle,
                                const glm::vec2& translation,
                                uint32_t textureHandle,
                                bgfx::ViewId viewId = 0);
    void setWindowSize(int width, int height);
    void enableScissor(bool enable);
    void setScissorRegion(const Rml::Rectanglei& region);
    void enableClipMask(bool enable);
    void renderToClipMask(Rml::ClipMaskOperation operation,
                          uint32_t geometryHandle,
                          const glm::vec2& translation,
                          bgfx::ViewId viewId);
    uint32_t loadTexture(const std::string& name,
                         const std::string& type,
                         const std::string& path,
                         uint16_t width,
                         uint16_t height);

  private:
    void updateOrtho();

    TextureLoader textureLoader;
    cfg::ConfigManager& config;

    con::ItemLib<Geometry> compiledGeometryLib;
    con::ItemLib<ShaderProgram> compiledShaderLib;

    bgfx::UniformHandle u_translation = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texColor = BGFX_INVALID_HANDLE;
    int winWidth;
    int winHeight;
    float ortho[16];
    bool scissorEnabled = false;
    Rml::Rectanglei scissorRegion;
    bool clipMaskEnabled = false;
    uint8_t stencilRef = 1;  // Current stencil reference value for testing
};

}  // namespace gfx

#endif