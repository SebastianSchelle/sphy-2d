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

class RenderEngine
{
  public:
    RenderEngine(cfg::ConfigManager& config);
    ~RenderEngine();
    
    // Prevent copying and moving (contains BGFX handles and other resources)
    RenderEngine(const RenderEngine&) = delete;
    RenderEngine& operator=(const RenderEngine&) = delete;
    RenderEngine(RenderEngine&&) = delete;
    RenderEngine& operator=(RenderEngine&&) = delete;

    void init();
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

  private:
    void updateOrtho();

    TextureLoader textureLoader;
    cfg::ConfigManager& config;

    con::ItemLib<Geometry> compiledGeometryLib;
    con::ItemLib<ShaderProgram> compiledShaderLib;

    bgfx::UniformHandle u_translation = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texArray = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texLayer = BGFX_INVALID_HANDLE;

    int winWidth;
    int winHeight;
    float ortho[16];
};

}  // namespace gfx

#endif