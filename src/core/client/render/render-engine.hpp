#ifndef RENDER_ENGINE_HPP
#define RENDER_ENGINE_HPP

#include "std-inc.hpp"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <item-lib.hpp>
#include <shader.hpp>

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
             bgfx::VertexLayout& vertLayout);

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
    RenderEngine();
    ~RenderEngine();

    void init();
    uint32_t compileGeometry(const void* vertexData,
                             size_t vDatSize,
                             const void* indexData,
                             size_t iDatSize,
                             bgfx::VertexLayout& vertLayout);
    void releaseGeometry(uint32_t handle);
    void renderCompiledGeometry(uint32_t handle,
                                const glm::vec2& translation,
                                uint32_t textureHandle);
    void setWindowSize(int width, int height);

  private:
    void updateOrtho();

    con::ItemLib<Geometry> compiledGeometryLib;
    con::ItemLib<ShaderProgram> compiledShaderLib;

    bgfx::UniformHandle u_translation = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texColor = BGFX_INVALID_HANDLE;
    int winWidth;
    int winHeight;
    float ortho[16];
};

}  // namespace gfx

#endif