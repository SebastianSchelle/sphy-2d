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

uint32_t RenderEngine::compileGeometry(const void* vertexData,
                                       size_t vDatSize,
                                       const void* indexData,
                                       size_t iDatSize,
                                       bgfx::VertexLayout& vertLayout,
                                       bool use32BitIndices)
{
    Geometry geometry(
        vertexData, vDatSize, indexData, iDatSize, vertLayout, use32BitIndices);
    con::IdxUuid idxUuid = compiledGeometryLib.addWithRandomKey(geometry);
    uint16_t gen = compiledGeometryLib.getGeneration(idxUuid.idx);
    return (uint32_t)(gen << 16) | (uint32_t)idxUuid.idx;
}

void RenderEngine::releaseGeometry(uint32_t handle)
{
    uint16_t index = handle & 0xffff;
    uint16_t generation = handle >> 16;
    const con::ItemWrapper<Geometry>* wrapper =
        compiledGeometryLib.getWrappedItem(index);
    if (wrapper && wrapper->generation == generation && wrapper->alive)
    {
        wrapper->item.destroy();
        compiledGeometryLib.removeItem(index);
    }
}

void RenderEngine::renderCompiledGeometry(uint32_t handle,
                                          const glm::vec2& translation,
                                          uint32_t textureHandle,
                                          bgfx::ViewId viewId)
{
    uint16_t geomIndex = handle & 0xffff;
    uint16_t geomGeneration = handle >> 16;

    uint16_t texIdx = textureHandle & 0xffff;
    uint16_t texGeneration = textureHandle >> 16;

    con::ItemWrapper<Geometry>* wrapper =
        compiledGeometryLib.getWrappedItem(geomIndex);
    if (!wrapper || !wrapper->alive || wrapper->generation != geomGeneration)
    {
        LG_W("Invalid geometry handle: {}", geomIndex);
        return;
    }
    const Geometry& geometry = wrapper->item;

    auto& texLib = textureLoader.getTextureLib();
    LG_D("TextureLib size: {}, texIdx: {}, texGeneration: {}",
         texLib.size(),
         texIdx,
         texGeneration);

    con::ItemWrapper<Texture>* texWrapper = texLib.getWrappedItem(texIdx);

    if (!texWrapper || !texWrapper->alive
        || texWrapper->generation != texGeneration)
    {
        LG_W(
            "Invalid texture handle: {} (lib size: {}, wrapper: {}, alive: {}, "
            "gen: {} vs {})",
            texIdx,
            texLib.size(),
            (void*)texWrapper,
            texWrapper ? texWrapper->alive : false,
            texWrapper ? texWrapper->generation : 0,
            texGeneration);
        return;
    }
    else
    {
        Texture& texture = texWrapper->item;
        bgfx::setTexture(0, u_texArray, texture.getTexIdent().texHandle);
        float layerArr[] = {static_cast<float>(texture.getTexIdent().layerIdx),
                            0.0f,
                            0.0f,
                            0.0f};
        bgfx::setUniform(u_texLayer, layerArr);
    }

    // Set uniforms
    float trArr[] = {translation.x, translation.y, 0.0f, 0.0f};
    bgfx::setUniform(u_translation, trArr);
    bgfx::setUniform(u_proj, ortho);

    // Set scissor region if enabled (per-draw scissor)
    // Only set scissor when enabled - don't set it when disabled to avoid
    // clipping
    if (scissorEnabled)
    {
        bgfx::setScissor((uint16_t)scissorRegion.Left(),
                         (uint16_t)scissorRegion.Top(),
                         (uint16_t)scissorRegion.Width(),
                         (uint16_t)scissorRegion.Height());
    }

    // Set render state
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                     | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                             BGFX_STATE_BLEND_INV_SRC_ALPHA);

    // Set stencil test if clip mask is enabled
    uint32_t fstencil = BGFX_STENCIL_NONE;
    if (clipMaskEnabled)
    {
        fstencil = BGFX_STENCIL_TEST_EQUAL | BGFX_STENCIL_FUNC_REF(stencilRef)
                   | BGFX_STENCIL_FUNC_RMASK(0xff) | BGFX_STENCIL_OP_FAIL_S_KEEP
                   | BGFX_STENCIL_OP_FAIL_Z_KEEP | BGFX_STENCIL_OP_PASS_Z_KEEP;
    }

    bgfx::setStencil(fstencil, BGFX_STENCIL_NONE);
    bgfx::setState(state);

    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, geometry.getVbh());
    bgfx::setIndexBuffer(geometry.getIbh());

    // Submit to the specified view
    bgfx::submit(viewId, compiledShaderLib.getItem(0)->getHandle());
}

void RenderEngine::enableScissor(bool enable)
{
    scissorEnabled = enable;
}

void RenderEngine::setScissorRegion(const Rml::Rectanglei& region)
{
    scissorRegion = region;
}

void RenderEngine::enableClipMask(bool enable)
{
    clipMaskEnabled = enable;
}

void RenderEngine::renderToClipMask(Rml::ClipMaskOperation operation,
                                    uint32_t geometryHandle,
                                    const glm::vec2& translation,
                                    bgfx::ViewId viewId)
{
    uint16_t index = geometryHandle & 0xffff;
    uint16_t generation = geometryHandle >> 16;
    con::ItemWrapper<Geometry>* wrapper =
        compiledGeometryLib.getWrappedItem(index);
    if (!wrapper || !wrapper->alive || wrapper->generation != generation)
    {
        LG_W("Invalid geometry handle for clip mask: {}", index);
        return;
    }
    const Geometry& geometry = wrapper->item;

    // Set view transform
    float view[16];
    float proj[16];
    bx::mtxIdentity(view);
    bx::mtxIdentity(proj);
    bgfx::setViewTransform(viewId, view, proj);

    // Set uniforms
    float trArr[] = {translation.x, translation.y, 0.0f, 0.0f};
    bgfx::setUniform(u_translation, trArr);
    bgfx::setUniform(u_proj, ortho);

    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, geometry.getVbh());
    bgfx::setIndexBuffer(geometry.getIbh());

    // Configure stencil based on operation
    uint32_t fstencil = BGFX_STENCIL_NONE;
    uint8_t writeValue = 1;

    switch (operation)
    {
        case Rml::ClipMaskOperation::Set:
            // Clear stencil to 0, then write 1 where geometry is
            bgfx::setViewClear(viewId, BGFX_CLEAR_STENCIL, 0, 1.0f, 0);
            fstencil =
                BGFX_STENCIL_TEST_ALWAYS | BGFX_STENCIL_FUNC_REF(writeValue)
                | BGFX_STENCIL_FUNC_RMASK(0xff) | BGFX_STENCIL_OP_FAIL_S_REPLACE
                | BGFX_STENCIL_OP_FAIL_Z_REPLACE
                | BGFX_STENCIL_OP_PASS_Z_REPLACE;
            stencilRef = writeValue;
            break;

        case Rml::ClipMaskOperation::SetInverse:
            // Clear stencil to 1, then write 0 where geometry is
            bgfx::setViewClear(viewId, BGFX_CLEAR_STENCIL, 0, 1.0f, 1);
            writeValue = 0;
            fstencil =
                BGFX_STENCIL_TEST_ALWAYS | BGFX_STENCIL_FUNC_REF(writeValue)
                | BGFX_STENCIL_FUNC_RMASK(0xff) | BGFX_STENCIL_OP_FAIL_S_REPLACE
                | BGFX_STENCIL_OP_FAIL_Z_REPLACE
                | BGFX_STENCIL_OP_PASS_Z_REPLACE;
            stencilRef = 0;
            break;

        case Rml::ClipMaskOperation::Intersect:
            // Increment stencil where geometry is
            fstencil =
                BGFX_STENCIL_TEST_ALWAYS | BGFX_STENCIL_FUNC_REF(0)
                | BGFX_STENCIL_FUNC_RMASK(0xff) | BGFX_STENCIL_OP_FAIL_S_KEEP
                | BGFX_STENCIL_OP_FAIL_Z_KEEP | BGFX_STENCIL_OP_PASS_Z_INCRSAT;
            stencilRef++;
            break;
    }

    // Render to stencil only (no color writes)
    bgfx::setStencil(fstencil, BGFX_STENCIL_NONE);
    bgfx::setState(0);  // No color/depth writes, just stencil

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

uint32_t RenderEngine::loadTexture(const std::string& name,
                                   const std::string& type,
                                   const std::string& path,
                                   uint16_t width,
                                   uint16_t height)
{
    // TextureLoader::loadTexture doesn't need width/height as it reads them
    // from the file
    return textureLoader.loadTexture(name, type, path);
}

}  // namespace gfx