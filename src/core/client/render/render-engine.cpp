#include "render-engine.hpp"
#include "vertex-defines.hpp"


namespace gfx
{

bgfx::VertexLayout VertexPosColTex::ms_decl;

Geometry::Geometry(const void* vertexData,
                   size_t vDatSize,
                   const void* indexData,
                   size_t iDatSize,
                   bgfx::VertexLayout& vertLayout)
{
    vbh = bgfx::createVertexBuffer(bgfx::makeRef(vertexData, vDatSize),
                                   vertLayout);
    ibh = bgfx::createIndexBuffer(bgfx::makeRef(indexData, iDatSize));
}

void Geometry::destroy()
{
    bgfx::destroy(vbh);
    bgfx::destroy(ibh);
}

bgfx::VertexBufferHandle Geometry::getVbh() const
{
    return vbh;
}

bgfx::IndexBufferHandle Geometry::getIbh() const
{
    return ibh;
}

RenderEngine::RenderEngine() {}

RenderEngine::~RenderEngine() {}

void RenderEngine::init()
{
    VertexPosColTex::init();

    ShaderProgram shaderProgram("modules/core/shader/vs_rmlui.bin",
                                "modules/core/shader/fs_rmlui.bin");
    compiledShaderLib.addItem("rmlui", shaderProgram);

    if (!bgfx::isValid(u_translation))
    {
        u_translation =
            bgfx::createUniform("u_translation", bgfx::UniformType::Vec4);
    }

    if (!bgfx::isValid(u_texColor))
    {
        u_texColor =
            bgfx::createUniform("u_texColor", bgfx::UniformType::Sampler);
    }
    // if (!bgfx::isValid(u_proj))
    // {
    //     u_proj =
    //         bgfx::createUniform("u_proj2", bgfx::UniformType::Mat4);
    // }

    updateOrtho();
}

uint32_t RenderEngine::compileGeometry(const void* vertexData,
                                       size_t vDatSize,
                                       const void* indexData,
                                       size_t iDatSize,
                                       bgfx::VertexLayout& vertLayout)
{
    VertexPosColTex* vertex = (VertexPosColTex*)vertexData;
    // for (int i = 0; i < vDatSize; ++i)
    // {
    //     LG_D("position: {}, {} color: {}, texcoord: {}, {}",
    //          vertex[i].x,
    //          vertex[i].y,
    //          vertex[i].rgba,
    //          vertex[i].u,
    //          vertex[i].v);
    // }
    Geometry geometry(vertexData, vDatSize, indexData, iDatSize, vertLayout);
    con::IdxUuid idxUuid = compiledGeometryLib.addWithRandomKey(geometry);
    uint16_t gen = compiledGeometryLib.getGeneration(idxUuid.idx);
    return (uint32_t)(gen << 16) | (uint32_t)idxUuid.idx;
}

void RenderEngine::releaseGeometry(uint32_t handle)
{
    uint16_t index = handle & 0xffff;
    uint16_t generation = handle >> 16;
    con::ItemWrapper<Geometry>* wrapper =
        compiledGeometryLib.getWrappedItem(index);
    if (wrapper && wrapper->generation == generation && wrapper->alive)
    {
        wrapper->item.destroy();
        compiledGeometryLib.removeItem(index);
    }
}

void RenderEngine::renderCompiledGeometry(uint32_t handle,
                                          const glm::vec2& translation,
                                          uint32_t textureHandle)
{
    uint16_t index = handle & 0xffff;
    uint16_t generation = handle >> 16;
    con::ItemWrapper<Geometry>* wrapper =
        compiledGeometryLib.getWrappedItem(index);
    if (!wrapper || !wrapper->alive || wrapper->generation != generation)
    {
        LG_W("Invalid geometry handle: {}", index);
        return;
    }
    const Geometry& geometry = wrapper->item;
    LG_W("Rendering geometry: {} {}",
         geometry.getVbh().idx,
         geometry.getIbh().idx);

    float trArr[] = {translation.x, translation.y, 0.0f, 0.0f};
    bgfx::setUniform(u_translation, &translation);
    // bgfx::setUniform(u_proj, ortho);

    bgfx::setVertexBuffer(0, geometry.getVbh());
    bgfx::setIndexBuffer(geometry.getIbh());
    bgfx::submit(0, compiledShaderLib.getItem(0)->getHandle());
}

void RenderEngine::setWindowSize(int width, int height)
{
    winWidth = width;
    winHeight = height;
    updateOrtho();
}

void RenderEngine::updateOrtho()
{
    bx::mtxOrtho(ortho,
                 0.0f,              // left
                 float(winWidth),   // right
                 float(winHeight),  // bottom
                 0.0f,              // top  (flip Y!)
                 0.0f,              // near
                 1000.0f,           // far
                 0.0f,              // offset
                 bgfx::getCaps()->homogeneousDepth);
}

}  // namespace gfx