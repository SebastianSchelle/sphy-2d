#include "render-engine.hpp"
#include "bgfx/bgfx.h"
#include "vertex-defines.hpp"

namespace gfx
{

bgfx::VertexLayout VertexPosColTex::ms_decl;
bgfx::VertexLayout PosVertex::ms_decl;
bgfx::VertexLayout PosColorVertex::ms_decl;

static const PosVertex vertRectangle[] = {{-0.5f, -0.5f},
                                          {0.5f, -0.5f},
                                          {-0.5f, 0.5f},
                                          {0.5f, 0.5f}};
static const uint16_t indRectangle[] = {2, 1, 0, 3, 1, 2};

static const PosVertex vertFullScreenTriangles[] = {{-1.0f, -1.0f},
                                                    {1.0f, -1.0f},
                                                    {-1.0f, 1.0f},
                                                    {1.0f, 1.0f}};
static const uint16_t indFullScreenTriangles[] = {2, 1, 0, 3, 1, 2};


Geometry::Geometry(const void* vertexData,
                   size_t vDatSize,
                   const void* indexData,
                   size_t iDatSize,
                   bgfx::VertexLayout& vertLayout,
                   bool use32BitIndices)
{
    vbh =
        bgfx::createVertexBuffer(bgfx::copy(vertexData, vDatSize), vertLayout);
    uint32_t flags = use32BitIndices ? BGFX_BUFFER_INDEX32 : 0;
    ibh = bgfx::createIndexBuffer(bgfx::copy(indexData, iDatSize), flags);
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

RenderEngine::RenderEngine(cfg::ConfigManager& config)
    : config(config), shaderHandleRml(ShaderHandle::Invalid()),
      textureHandleFallback(TextureHandle::Invalid())
{
}

RenderEngine::~RenderEngine()
{
    cleanUpAll();
    LG_D("RenderEngine destroyed");
}

bool RenderEngine::initPre()
{
    VertexPosColTex::init();

    texWidth =
        static_cast<int>(std::get<float>(config.get({"gfx", "tex-width"})));
    texHeight =
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

    shaderHandleRml = loadShader("geom",
                                 "modules/engine/shader/vs_rmlui.bin",
                                 "modules/engine/shader/fs_rmlui.bin");


    PosColorVertex::init();
    PosVertex::init();
    vbhRectangle = bgfx::createVertexBuffer(
        bgfx::copy(vertRectangle, sizeof(vertRectangle)),
        PosColorVertex::ms_decl);
    ibhRectangle = bgfx::createIndexBuffer(
        bgfx::copy(indRectangle, sizeof(indRectangle)), 0);

    vbhFullScreenTriangles = bgfx::createVertexBuffer(
        bgfx::copy(vertFullScreenTriangles, sizeof(vertFullScreenTriangles)),
        PosVertex::ms_decl);
    ibhFullScreenTriangles = bgfx::createIndexBuffer(
        bgfx::copy(indFullScreenTriangles, sizeof(indFullScreenTriangles)), 0);

    if (!shaderHandleRml.isValid())
    {
        LG_E("Failed to load rmlui shader");
        return false;
    }

    textureHandleFallback = textureLoader.loadTexture(
        "fallback", "misc", "modules/engine/assets/textures/fallback.dds");
    if (!textureHandleFallback.isValid())
    {
        LG_E("Failed to load fallback texture");
        return false;
    }

    if (!bgfx::isValid(u_translation))
    {
        u_translation =
            bgfx::createUniform("u_translation", bgfx::UniformType::Vec4);
    }

    if (!bgfx::isValid(u_atlasPos))
    {
        u_atlasPos = bgfx::createUniform("u_atlasPos", bgfx::UniformType::Vec4);
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
    return true;
}

bool RenderEngine::initPost()
{
    return true;
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
    GeometryHandleUuid handle = compiledGeometryLib.addWithRandomKey(geometry);
    return handle.handle;
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

void RenderEngine::renderCompiledGeometry(GeometryHandle goemHandle,
                                          const glm::vec2& translation,
                                          TextureHandle textureHandle,
                                          bgfx::ViewId viewId)
{
    changeRenderState(RenderState::DrawCompiledGeometry);

    const Geometry* geometry = compiledGeometryLib.getItem(goemHandle);
    if (!geometry)
    {
        LG_W("Invalid geometry handle: {}", goemHandle.value());
        return;
    }

    auto& texLib = textureLoader.getTextureLib();
    Texture* texture = texLib.getItem(textureHandle);
    if (!texture)
    {
        // LG_W("Invalid texture handle: id: {} gen: {}. Try fallback texture",
        //      textureHandle.getIdx(),
        //      textureHandle.getGeneration());
        texture = texLib.getItem(0);  // Fallback texture
        if (!texture)
        {
            LG_W("No fallback texture found");
            return;
        }
    }

    bgfx::setTexture(0,
                     u_texArray,
                     texture->getTexIdent().texHandle,
                     BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
                         | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
                         | BGFX_SAMPLER_MIP_POINT);

    float layerArr[] = {
        static_cast<float>(texture->getTexIdent().layerIdx), 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(u_texLayer, layerArr);

    float atlasPos[] = {texture->getRelBounds().x,
                        texture->getRelBounds().y,
                        texture->getRelBounds().z,
                        texture->getRelBounds().w};

    bgfx::setUniform(u_atlasPos, atlasPos);

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
    bgfx::submit(viewId,
                 compiledShaderLib.getItem(shaderHandleRml)->getHandle());
}

void RenderEngine::setWindowSize(int width, int height)
{
    winWidth = width;
    winHeight = height;
    updateOrtho();
    bgfx::setViewRect(kClearView, 0, 0, bgfx::BackbufferRatio::Equal);
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

TextureHandle RenderEngine::loadTexture(const std::string& name,
                                        const std::string& type,
                                        const std::string& path,
                                        glm::vec2& dimensions)
{
    return textureLoader.loadTexture(name, type, path, dimensions);
}

TextureHandle RenderEngine::generateTexture(const std::string& name,
                                            const std::string& type,
                                            const void* data,
                                            int width,
                                            int height,
                                            const std::string& path)
{
    return textureLoader.generateTexture(name, type, data, width, height, path);
}

void RenderEngine::setScissorRegion(const glm::vec2& position,
                                    const glm::vec2& size)
{
    scissorRegionPosition = position;
    scissorRegionSize = size;
    if (scissorRegionEnabled)
    {
        bgfx::setScissor(scissorRegionPosition.x,
                         scissorRegionPosition.y,
                         scissorRegionSize.x,
                         scissorRegionSize.y);
    }
    else
    {
        bgfx::setScissor(0, 0, winWidth, winHeight);
    }
}

void RenderEngine::enableScissorRegion(bool enable)
{
    scissorRegionEnabled = enable;
    if (scissorRegionEnabled)
    {
        bgfx::setScissor(scissorRegionPosition.x,
                         scissorRegionPosition.y,
                         scissorRegionSize.x,
                         scissorRegionSize.y);
    }
    else
    {
        bgfx::setScissor(0, 0, winWidth, winHeight);
    }
}

glm::ivec2 RenderEngine::getTextureSize() const
{
    return glm::ivec2(texWidth, texHeight);
}

void RenderEngine::cleanUpAll()
{
    cleanUpTextures();
    cleanUpShaders();
    cleanUpGeometry();
}

void RenderEngine::cleanUpTextures() {}

void RenderEngine::cleanUpShaders()
{
    ShaderHandle shaderHandle = ShaderHandle::Invalid();
    while ((shaderHandle = compiledShaderLib.firstAliveHandle()).isValid())
    {
        compiledShaderLib.getItem(shaderHandle)->destroy();
        compiledShaderLib.removeItem(shaderHandle);
    }
}

void RenderEngine::cleanUpGeometry()
{
    // compiledGeometryLib.clear();
}

ShaderHandle RenderEngine::loadShader(const std::string& name,
                                      const std::string& vsPath,
                                      const std::string& fsPath)
{
    const ShaderProgram shaderProgram = ShaderProgram(vsPath, fsPath);
    return compiledShaderLib.addItem(name, shaderProgram);
}

void RenderEngine::releaseShader(ShaderHandle handle)
{
    compiledShaderLib.removeItem(handle);
}

void RenderEngine::changeRenderState(RenderState newState)
{
    if (renderState != newState)
    {
        switch ((int)renderState)
        {
            case (int)RenderState::Idle:
                break;
            case (int)RenderState::DrawTexRects:
                break;
            case (int)RenderState::DrawFullScreenTriangles:
                break;
            case (int)RenderState::DrawCompiledGeometry:
                break;
            default:
                break;
        }
        renderState = newState;
    }
}

void RenderEngine::startFrame()
{
    renderState = RenderState::Idle;

    bool showStats = false;
    bgfx::setDebug(showStats ? BGFX_DEBUG_STATS | BGFX_DEBUG_TEXT : 0);
    bgfx::touch(0);

    bgfx::setViewClear(
        kClearView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
}

void RenderEngine::drawFullScreenTriangles(bgfx::ViewId viewId,
                                           ShaderHandle shaderHandle)
{
    if (shaderHandle.isValid())
    {
        changeRenderState(RenderState::DrawFullScreenTriangles);
        bgfx::setVertexBuffer(0, vbhFullScreenTriangles);
        bgfx::setIndexBuffer(ibhFullScreenTriangles);
        bgfx::setState(BGFX_STATE_WRITE_RGB);
        bgfx::submit(viewId,
                     compiledShaderLib.getItem(shaderHandle)->getHandle());
    }
}

ShaderHandle RenderEngine::getShaderHandle(const std::string& name)
{
    return compiledShaderLib.getHandle(name);
}

}  // namespace gfx
// Explicitly instantiate ItemLib<Geometry> to ensure Handle is fully defined
// Must be outside the gfx namespace
template class con::ItemLib<gfx::Geometry>;