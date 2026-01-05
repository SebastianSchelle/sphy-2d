#include "render-engine.hpp"
#include "bgfx/bgfx.h"
#include "vertex-defines.hpp"

namespace gfx
{

bgfx::VertexLayout VertexPosColTex::ms_decl;

Geometry::Geometry(const void* vertexData,
                   size_t vDatSize,
                   const void* indexData,
                   size_t iDatSize,
                   bgfx::VertexLayout& vertLayout,
                   bool use32BitIndices)
{
    vbh = bgfx::createVertexBuffer(bgfx::makeRef(vertexData, vDatSize),
                                   vertLayout);
    uint32_t flags = use32BitIndices ? BGFX_BUFFER_INDEX32 : 0;
    ibh = bgfx::createIndexBuffer(bgfx::makeRef(indexData, iDatSize), flags);
}

void Geometry::destroy() const
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

RenderEngine::RenderEngine(cfg::ConfigManager& config) : config(config) {}

RenderEngine::~RenderEngine() {}

void RenderEngine::init()
{
    VertexPosColTex::init();

    ShaderProgram shaderProgram("modules/core/shader/vs_rmlui.bin",
                                "modules/core/shader/fs_rmlui.bin");
    compiledShaderLib.addItem("rmlui", shaderProgram);

    int texWidth =
        static_cast<int>(std::get<float>(config.get({"gfx", "tex-width"})));
    int texHeight =
        static_cast<int>(std::get<float>(config.get({"gfx", "tex-height"})));
    int texLayerCnt =
        static_cast<int>(std::get<float>(config.get({"gfx", "tex-layer-cnt"})));
    int texBucketSize = static_cast<int>(
        std::get<float>(config.get({"gfx", "tex-bucket-size"})));
    float texExcessHeightThreshold = static_cast<float>(
        std::get<float>(config.get({"gfx", "tex-excess-height-threshold"})));

    textureLoader.init(texWidth,
                       texHeight,
                       texLayerCnt,
                       texBucketSize,
                       texExcessHeightThreshold);

    // uint32_t fallbackTexHandle = textureLoader.loadTexture(
    //     "fallback", "misc", "modules/core/texture/fallback.dds");


    if (!bgfx::isValid(u_translation))
    {
        u_translation =
            bgfx::createUniform("u_translation", bgfx::UniformType::Vec4);
    }

    if (!bgfx::isValid(u_texArray))
    {
        u_texArray =
            bgfx::createUniform("u_texArray", bgfx::UniformType::Sampler);
    }

    if (!bgfx::isValid(u_texLayer))
    {
        u_texLayer = bgfx::createUniform("u_texLayer", bgfx::UniformType::Vec4);
    }

    if (!bgfx::isValid(u_proj))
    {
        u_proj = bgfx::createUniform("u_myproj", bgfx::UniformType::Mat4);
    }

    // Initialize with default size (will be updated when window size is known)
    winWidth = 800;
    winHeight = 600;
    updateOrtho();
}

GeometryHandle RenderEngine::compileGeometry(const void* vertexData,
                                            size_t vDatSize,
                                            const void* indexData,
                                            size_t iDatSize,
                                            bgfx::VertexLayout& vertLayout,
                                            bool use32BitIndices)
{
    Geometry geometry(
        vertexData, vDatSize, indexData, iDatSize, vertLayout, use32BitIndices);
    con::IdxUuid idxUuid = compiledGeometryLib.addWithRandomKey(geometry);
    return compiledGeometryLib.getHandle(idxUuid.idx);
}

void RenderEngine::releaseGeometry(GeometryHandle handle)
{
    const Geometry* geometry = compiledGeometryLib.getItem(handle);
    if (geometry)
    {
        geometry->destroy();
        compiledGeometryLib.removeItem(handle);
    }
}

void RenderEngine::renderCompiledGeometry(GeometryHandle handle,
                                         const glm::vec2& translation,
                                         TextureHandle textureHandle,
                                         bgfx::ViewId viewId)
{
    const Geometry* geometry = compiledGeometryLib.getItem(handle);
    if (!geometry)
    {
        LG_W("Invalid geometry handle: {}", handle.value());
        return;
    }

    auto& texLib = textureLoader.getTextureLib();
    Texture* texture = texLib.getItem(textureHandle);
    if (!texture)
    {
        texture = texLib.getItem(0);  // Fallback texture
        if (!texture)
        {
            LG_W("No fallback texture found");
            return;
        }
    }

    bgfx::setTexture(0, u_texArray, texture->getTexIdent().texHandle);
    float layerArr[] = {
        static_cast<float>(texture->getTexIdent().layerIdx), 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(u_texLayer, layerArr);

    // Set uniforms
    float trArr[] = {translation.x, translation.y, 0.0f, 0.0f};
    bgfx::setUniform(u_translation, trArr);
    bgfx::setUniform(u_proj, ortho);

    // Set render state
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                     | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                             BGFX_STATE_BLEND_INV_SRC_ALPHA);

    bgfx::setState(state);

    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, geometry->getVbh());
    bgfx::setIndexBuffer(geometry->getIbh());

    // Submit to the specified view
    bgfx::submit(viewId, compiledShaderLib.getItem(0)->getHandle());
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

TextureHandle RenderEngine::loadTexture(const std::string& name,
                                        const std::string& type,
                                        const std::string& path)
{
    return textureLoader.loadTexture(name, type, path);
}

}  // namespace gfx

// Explicitly instantiate ItemLib<Geometry> to ensure Handle is fully defined
// Must be outside the gfx namespace
template class con::ItemLib<gfx::Geometry>;