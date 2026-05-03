#include "render-engine.hpp"
#include "bgfx/bgfx.h"
#include "config-manager.hpp"
#include "logging.hpp"
#include "std-inc.hpp"
#include "vertex-defines.hpp"
#include <algorithm>

namespace gfx
{

// Atlas sampling: full POINT (MIN|MAG|MIP) makes texels snap hard to the grid.
// With fractional screen positions (camera zoom/pan), that shows up as shimmer /
// crawl. MIN_ANISOTROPIC + MAG_POINT is a common 2D tradeoff: sharper when
// zoomed in, smoother minification when zoomed out. For pixel-perfect art at
// integer scales only, use BGFX_SAMPLER_POINT + UVW_CLAMP instead.
static constexpr uint32_t kSpriteSamplerFlags =
    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    | BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_POINT;

static_assert(
    sizeof(TexRectData) % 16 == 0,
    "TexRectData stride must be a multiple of 16 for bgfx instancing");

bgfx::VertexLayout VertexPosColTex::ms_decl;
bgfx::VertexLayout PosVertex::ms_decl;
bgfx::VertexLayout PosColorVertex::ms_decl;
bgfx::VertexLayout PosColorShapeVertex::ms_decl;

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

void Geometry::destroy()
{
    if (bgfx::isValid(vbh))
    {
        bgfx::destroy(vbh);
        vbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(ibh))
    {
        bgfx::destroy(ibh);
        ibh = BGFX_INVALID_HANDLE;
    }
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
    : config(config), textureHandleFallback(TextureHandle::Invalid())
{
}

RenderEngine::~RenderEngine()
{
    shutdown();
    LG_D("RenderEngine destroyed");
}

void RenderEngine::shutdown()
{
    if (hasShutdown)
    {
        return;
    }
    hasShutdown = true;
    cleanUpAll();
}

bool RenderEngine::initPre()
{
    startTime = tim::getCurrentTimeU();

    VertexPosColTex::init();

    texWidth = CFG_INT(config, 2000.0f, "gfx", "tex-width");
    texHeight = CFG_INT(config, 2000.0f, "gfx", "tex-height");
    const int texLayerCnt = CFG_INT(config, 16.0f, "gfx", "tex-layer-cnt");
    const int texBucketSize =
        CFG_INT(config, 1000.0f, "gfx", "tex-bucket-size");
    const float texExcessHeightThreshold =
        CFG_FLOAT(config, 0.8f, "gfx", "tex-excess-height-threshold");
    zoomPanCfgWorld.zoomStep = CFG_FLOAT(config, 0.1f, "ui", "zoom", "step");
    zoomPanCfgWorld.maxZoom = CFG_FLOAT(config, 10.0f, "ui", "zoom", "max");
    zoomPanCfgWorld.minZoom = CFG_FLOAT(config, 0.01f, "ui", "zoom", "min");
    zoomPanCfgWorld.panSpeed = CFG_FLOAT(config, 10.0f, "ui", "pan", "speed");
    zoomTactical = CFG_FLOAT(config, 0.5f, "ui", "zoom", "tactical-map");
    zoomStrategic = CFG_FLOAT(config, 0.2f, "ui", "zoom", "strategic-map");
    maxTexPerDrawCall =
        CFG_INT(config, 1024.0f, "gfx", "max-tex-per-draw-call");

    textureLoader.init(texWidth,
                       texHeight,
                       texLayerCnt,
                       texBucketSize,
                       texExcessHeightThreshold);

    PosColorVertex::init();
    PosVertex::init();
    PosColorShapeVertex::init();

    vbhRectangle = bgfx::createVertexBuffer(
        bgfx::copy(vertRectangle, sizeof(vertRectangle)), PosVertex::ms_decl);
    ibhRectangle = bgfx::createIndexBuffer(
        bgfx::copy(indRectangle, sizeof(indRectangle)), 0);

    vbhFullScreenTriangles = bgfx::createVertexBuffer(
        bgfx::copy(vertFullScreenTriangles, sizeof(vertFullScreenTriangles)),
        PosVertex::ms_decl);
    ibhFullScreenTriangles = bgfx::createIndexBuffer(
        bgfx::copy(indFullScreenTriangles, sizeof(indFullScreenTriangles)), 0);

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

    if (!bgfx::isValid(u_time))
    {
        u_time = bgfx::createUniform("u_time", bgfx::UniformType::Vec4);
    }

    if (!bgfx::isValid(u_grid))
    {
        u_grid = bgfx::createUniform("u_grid", bgfx::UniformType::Vec4);
    }

    if (!bgfx::isValid(u_transform))
    {
        u_transform =
            bgfx::createUniform("u_transform", bgfx::UniformType::Mat4);
    }

    // Initialize with default size (will be updated when window size is known)
    winWidth = 800;
    winHeight = 600;
    updateOrtho();

    geomTransformMatrix = glm::mat4(1.0f);

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
    Geometry* geometry = compiledGeometryLib.getItem(handle);
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
    if (!shaderHandleRml.isValid())
    {
        shaderHandleRml = getShaderHandle("geom");
        return;
    }

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
        texture = texLib.getItem(textureHandleFallback);  // Fallback texture
        if (!texture)
        {
            textureHandleFallback =
                textureLoader.getTextureLib().getHandle("fallback");
            return;
        }
    }

    bgfx::setTexture(0,
                     u_texArray,
                     texture->getTexIdent().texHandle,
                     kSpriteSamplerFlags);

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
    const float* projForView = (viewId == kWorldView) ? worldViewProj : ortho;
    bgfx::setUniform(u_proj, projForView);

    bgfx::setUniform(u_transform, &geomTransformMatrix);

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
    bgfx::setViewRect(kWorldView, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(kUiView, 0, 0, bgfx::BackbufferRatio::Equal);
}

void RenderEngine::setWorldCamera(const vec2& position, float zoom)
{
    worldCameraX = position.x;
    worldCameraY = position.y;
    worldZoom = zoom;
}

vec2 RenderEngine::screenToWorldPixel(const vec2& screenPx) const
{
    const float wf = float(winWidth);
    const float hf = float(winHeight);
    const float ndcX = 2.f * screenPx.x / wf - 1.f;
    const float ndcY = 1.f - 2.f * screenPx.y / hf;

    float clip[4] = {ndcX, ndcY, 0.0f, 1.0f};

    float worldH[4];
    bx::vec4MulMtx(worldH, clip, invWvp);

    const float outW = worldH[3];
    if (bx::abs(outW) > 1e-6f)
    {
        return {worldH[0] / outW, worldH[1] / outW};
    }
    return {worldH[0], worldH[1]};
}

vec2 RenderEngine::screenToWorldRel(const vec2& screenPosRel) const
{
    float clip[4] = {screenPosRel.x, screenPosRel.y, 0.0f, 1.0f};

    float worldH[4];
    bx::vec4MulMtx(worldH, clip, invWvp);

    const float outW = worldH[3];
    if (bx::abs(outW) > 1e-6f)
    {
        return {worldH[0] / outW, worldH[1] / outW};
    }
    return {worldH[0], worldH[1]};
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

void RenderEngine::updateWorldView()
{
    float scaleMtx[16];
    float transMtx[16];
    float trX = -worldCameraX + (winWidth * 0.5f) / worldZoom;
    float trY = -worldCameraY + (winHeight * 0.5f) / worldZoom;
    bx::mtxScale(scaleMtx, worldZoom, worldZoom, 1.0f);
    bx::mtxTranslate(transMtx, trX, trY, 0.0f);
    bx::mtxMul(worldView, transMtx, scaleMtx);
    bx::mtxMul(worldViewProj, worldView, ortho);
    bx::mtxInverse(invWvp, worldViewProj);

    viewMode = worldZoom < zoomTactical    ? GameViewMode::StrategicMap
               : worldZoom < zoomStrategic ? GameViewMode::TacticalMap
                                           : GameViewMode::ThirdPerson;
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

void RenderEngine::setTransform(const glm::mat4& transform)
{
    geomTransformMatrix = transform;
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

    if (bgfx::isValid(vbhRectangle))
    {
        bgfx::destroy(vbhRectangle);
        vbhRectangle = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(ibhRectangle))
    {
        bgfx::destroy(ibhRectangle);
        ibhRectangle = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(vbhFullScreenTriangles))
    {
        bgfx::destroy(vbhFullScreenTriangles);
        vbhFullScreenTriangles = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(ibhFullScreenTriangles))
    {
        bgfx::destroy(ibhFullScreenTriangles);
        ibhFullScreenTriangles = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(u_translation))
    {
        bgfx::destroy(u_translation);
        u_translation = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_proj))
    {
        bgfx::destroy(u_proj);
        u_proj = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_texArray))
    {
        bgfx::destroy(u_texArray);
        u_texArray = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_texLayer))
    {
        bgfx::destroy(u_texLayer);
        u_texLayer = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_atlasPos))
    {
        bgfx::destroy(u_atlasPos);
        u_atlasPos = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_time))
    {
        bgfx::destroy(u_time);
        u_time = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_grid))
    {
        bgfx::destroy(u_grid);
        u_grid = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(u_transform))
    {
        bgfx::destroy(u_transform);
        u_transform = BGFX_INVALID_HANDLE;
    }
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
    GeometryHandle handle = GeometryHandle::Invalid();
    while ((handle = compiledGeometryLib.firstAliveHandle()).isValid())
    {
        Geometry* geometry = compiledGeometryLib.getItem(handle);
        if (geometry)
        {
            geometry->destroy();
        }
        compiledGeometryLib.removeItem(handle);
    }
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
                submitTexRects();
                break;
            case (int)RenderState::DrawFullScreenTriangles:
                break;
            case (int)RenderState::DrawCompiledGeometry:
                break;
            case (int)RenderState::DrawShapes:
                submitShapes();
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
    bgfx::touch(kWorldView);
    bgfx::touch(kUiView);

    bgfx::setViewClear(
        kWorldView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(kWorldView, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(kUiView, 0, 0, bgfx::BackbufferRatio::Equal);
    updateWorldView();
    bgfx::setViewTransform(kWorldView, worldView, ortho);

    tim::Timepoint now = tim::getCurrentTimeU();
    frameTime = (float)tim::durationU(startTime, now) / 1000000.0f;
    float timeVec[4] = {frameTime, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(u_time, timeVec);
}

void RenderEngine::endFrame()
{
    changeRenderState(RenderState::Idle);
    bgfx::frame();
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

void RenderEngine::drawBlueprintGridBackground(bgfx::ViewId viewId,
                                               ShaderHandle shaderHandle,
                                               float cellWorld,
                                               float majorEveryCells)
{
    if (!shaderHandle.isValid() || !bgfx::isValid(u_grid))
    {
        return;
    }
    const float cell = cellWorld < 0.25f ? 0.25f : cellWorld;
    const float major = majorEveryCells < 1.0f ? 1.0f : majorEveryCells;
    const float gridParams[4] = {cell, major, 0.0f, 0.0f};
    bgfx::setUniform(u_grid, gridParams);
    drawFullScreenTriangles(viewId, shaderHandle);
}

ShaderHandle RenderEngine::getShaderHandle(const std::string& name)
{
    return compiledShaderLib.getHandle(name);
}

void RenderEngine::drawBoxShape(float shapeType,
                                const glm::vec2& pos,
                                const glm::vec2& size,
                                uint32_t colorRGBA,
                                float thickness,
                                float rotationRad,
                                float zIndex,
                                bgfx::ViewId viewId)
{
    changeRenderState(RenderState::DrawShapes);
    currentViewId = viewId;
    if (currentShapeCount >= MAX_SHAPES)
    {
        submitShapes();
    }
    if (currentShapeCount == 0)
    {
        allocateForShapes();
    }

    float thicknessX = 2.0f * thickness / size.x;
    float thicknessY = 2.0f * thickness / size.y;

    PosColorShapeVertex* vertices = (PosColorShapeVertex*)tvbSdf.data;
    vec2 hs = size / 2.0f;
    const float cx = pos.x, cy = pos.y;

    vertices[currentShapeVertices++] = PosColorShapeVertex{-hs.x,
                                                           -hs.y,
                                                           -1.0f,
                                                           -1.0f,
                                                           colorRGBA,
                                                           shapeType,
                                                           thicknessX,
                                                           thicknessY,
                                                           cx,
                                                           cy,
                                                           rotationRad,
                                                           zIndex};
    vertices[currentShapeVertices++] = PosColorShapeVertex{hs.x,
                                                           -hs.y,
                                                           1.0f,
                                                           -1.0f,
                                                           colorRGBA,
                                                           shapeType,
                                                           thicknessX,
                                                           thicknessY,
                                                           cx,
                                                           cy,
                                                           rotationRad,
                                                           zIndex};
    vertices[currentShapeVertices++] = PosColorShapeVertex{-hs.x,
                                                           hs.y,
                                                           -1.0f,
                                                           1.0f,
                                                           colorRGBA,
                                                           shapeType,
                                                           thicknessX,
                                                           thicknessY,
                                                           cx,
                                                           cy,
                                                           rotationRad,
                                                           zIndex};
    vertices[currentShapeVertices++] = PosColorShapeVertex{hs.x,
                                                           hs.y,
                                                           1.0f,
                                                           1.0f,
                                                           colorRGBA,
                                                           shapeType,
                                                           thicknessX,
                                                           thicknessY,
                                                           cx,
                                                           cy,
                                                           rotationRad,
                                                           zIndex};
    uint16_t* indices = (uint16_t*)tibSdf.data;
    indices[currentShapeIndices++] = currentShapeVertices - 4;
    indices[currentShapeIndices++] = currentShapeVertices - 3;
    indices[currentShapeIndices++] = currentShapeVertices - 2;
    indices[currentShapeIndices++] = currentShapeVertices - 3;
    indices[currentShapeIndices++] = currentShapeVertices - 1;
    indices[currentShapeIndices++] = currentShapeVertices - 2;
    currentShapeCount++;
}

void RenderEngine::drawEllipse(const glm::vec2& pos,
                               const glm::vec2& size,
                               uint32_t colorRGBA,
                               float thickness,
                               float rotationRad,
                               float zIndex,
                               bgfx::ViewId viewId)
{
    drawBoxShape(SHAPE_TYPE_CIRCLE,
                 pos,
                 size,
                 colorRGBA,
                 thickness,
                 rotationRad,
                 zIndex,
                 viewId);
}

void RenderEngine::drawRectangle(const glm::vec2& pos,
                                 const glm::vec2& size,
                                 uint32_t colorRGBA,
                                 float thickness,
                                 float rotationRad,
                                 float zIndex,
                                 bgfx::ViewId viewId)
{
    drawBoxShape(SHAPE_TYPE_RECTANGLE,
                 pos,
                 size,
                 colorRGBA,
                 thickness,
                 rotationRad,
                 zIndex,
                 viewId);
}

tim::Timepoint RenderEngine::getStartTime() const
{
    return startTime;
}

void RenderEngine::allocateForShapes()
{
    if (bgfx::getAvailTransientVertexBuffer(MAX_SHAPES * 4,
                                            PosColorShapeVertex::ms_decl)
        && bgfx::getAvailTransientIndexBuffer(MAX_SHAPES * 6, false))
    {
        bgfx::allocTransientVertexBuffer(
            &tvbSdf, MAX_SHAPES * 4, PosColorShapeVertex::ms_decl);
        bgfx::allocTransientIndexBuffer(&tibSdf, MAX_SHAPES * 6, false);
    }
}

void RenderEngine::submitShapes()
{
    if (currentShapeCount > 0)
    {
        if (!shaderHandleShapes.isValid())
        {
            shaderHandleShapes = getShaderHandle("sdf-shapes");
            return;
        }
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                         | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS
                         | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                 BGFX_STATE_BLEND_INV_SRC_ALPHA);

        const float* projForView =
            (currentViewId == kWorldView) ? worldViewProj : ortho;
        bgfx::setUniform(u_proj, projForView);
        bgfx::setState(state);
        bgfx::setVertexBuffer(0, &tvbSdf, 0, currentShapeVertices);
        bgfx::setIndexBuffer(&tibSdf, 0, currentShapeIndices);
        bgfx::submit(
            currentViewId,
            compiledShaderLib.getItem(shaderHandleShapes)->getHandle());
        currentShapeCount = 0;
        currentShapeVertices = 0;
        currentShapeIndices = 0;
    }
}

void RenderEngine::drawTexRect(const glm::vec2& pos,
                               const glm::vec2& size,
                               TextureHandle textureHandle,
                               float rotationRad,
                               uint32_t colorABGR,
                               float zIndex,
                               bgfx::ViewId viewId)
{
    changeRenderState(RenderState::DrawTexRects);
    currentViewId = viewId;

    auto& texLib = textureLoader.getTextureLib();
    Texture* texture = texLib.getItem(textureHandle);
    if (!texture)
    {
        texture = texLib.getItem(textureHandleFallback);
        if (!texture)
        {
            return;
        }
    }

    const bgfx::TextureHandle arrayHandle = texture->getTexIdent().texHandle;
    if (currentTexRectCount > 0 && bgfx::isValid(texRectBatchArray)
        && texRectBatchArray.idx != arrayHandle.idx)
    {
        submitTexRects();
    }

    if (currentTexRectCount >= maxTexPerDrawCall)
    {
        submitTexRects();
    }

    if (currentTexRectCount == 0)
    {
        allocateForTexRects();
        if (idbTex.data == nullptr)
        {
            return;
        }
        texRectBatchArray = arrayHandle;
    }

    uint8_t a = (colorABGR >> 24) & 0xff;
    uint8_t b = (colorABGR >> 16) & 0xff;
    uint8_t g = (colorABGR >> 8) & 0xff;
    uint8_t r = (colorABGR >> 0) & 0xff;

    TexRectData* inst =
        reinterpret_cast<TexRectData*>(idbTex.data) + currentTexRectCount;
    inst->rect = vec4(pos.x, pos.y, size.x, size.y);
    inst->colorAbgr = vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    inst->atlasUv = glm::vec4(texture->getRelBounds());
    inst->rotLayZ = vec4(rotationRad,
                         static_cast<float>(texture->getTexIdent().layerIdx),
                         zIndex,
                         0.0f);
    currentTexRectCount++;
}

void RenderEngine::submitTexRects()
{
    if (currentTexRectCount == 0 || !bgfx::isValid(texRectBatchArray)
        || idbTex.data == nullptr)
    {
        currentTexRectCount = 0;
        texRectBatchArray = BGFX_INVALID_HANDLE;
        LG_W("Failed to submit texrects");
        return;
    }

    if (!shaderHandleTexRect.isValid())
    {
        shaderHandleTexRect = getShaderHandle("texrect");
        LG_W("Failed to get shader handle for texrect");
        return;
    }

    bgfx::setTexture(0,
                     u_texArray,
                     texRectBatchArray,
                     kSpriteSamplerFlags);

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                     | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS
                     | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                             BGFX_STATE_BLEND_INV_SRC_ALPHA);

    const float* projForView =
        (currentViewId == kWorldView) ? worldViewProj : ortho;
    bgfx::setUniform(u_proj, projForView);
    bgfx::setState(state);
    bgfx::setVertexBuffer(0, vbhRectangle);
    bgfx::setIndexBuffer(ibhRectangle);
    bgfx::setInstanceDataBuffer(&idbTex, 0, (uint32_t)currentTexRectCount);

    bgfx::submit(currentViewId,
                 compiledShaderLib.getItem(shaderHandleTexRect)->getHandle());

    currentTexRectCount = 0;
    texRectBatchArray = BGFX_INVALID_HANDLE;
}

void RenderEngine::allocateForTexRects()
{
    const uint16_t stride = static_cast<uint16_t>(sizeof(TexRectData));
    const uint32_t want = static_cast<uint32_t>(maxTexPerDrawCall);
    const uint32_t avail = bgfx::getAvailInstanceDataBuffer(want, stride);
    if (avail == 0)
    {
        return;
    }
    const uint32_t n = std::min(want, avail);
    bgfx::allocInstanceDataBuffer(&idbTex, n, stride);
}

void RenderEngine::zoomWorld(float amount)
{
    for (int i = 0; i < abs(amount); i++)
    {
        if (amount > 0)
        {
            worldZoom *= zoomPanCfgWorld.zoomStep;
        }
        else
        {
            worldZoom /= zoomPanCfgWorld.zoomStep;
        }
    }
    if (amount > 0)
    {
        if (worldZoom > zoomPanCfgWorld.maxZoom)
        {
            worldZoom = zoomPanCfgWorld.maxZoom;
        }
    }
    else
    {
        if (worldZoom < zoomPanCfgWorld.minZoom)
        {
            worldZoom = zoomPanCfgWorld.minZoom;
        }
    }
}

void RenderEngine::panWorld(PanDirection dirX, PanDirection dirY)
{
    worldCameraX += (float)dirX * zoomPanCfgWorld.panSpeed / worldZoom;
    worldCameraY += (float)dirY * zoomPanCfgWorld.panSpeed / worldZoom;
    updatePosWithSectorOffset();
}

void RenderEngine::panWorld(const glm::vec2& delta)
{
    worldCameraX += delta.x;
    worldCameraY += delta.y;
    updatePosWithSectorOffset();
}

void RenderEngine::setWorldCameraPosition(const glm::vec2& position)
{
    worldCameraX = position.x;
    worldCameraY = position.y;
    updatePosWithSectorOffset();
}

void RenderEngine::setWorldShape(const def::WorldShape* worldShape)
{
    this->worldShape = worldShape;
}

void RenderEngine::updatePosWithSectorOffset()
{
    if (!worldShape)
    {
        return;
    }
    int deltaOffsX =
        std::clamp((int)floorf(worldCameraX / worldShape->sectorSize + 0.5f),
                   (int32_t)(-sectorOffsetX),
                   (int32_t)(worldShape->numSectorX - 1) - sectorOffsetX);
    int deltaOffsY =
        std::clamp((int)floorf(worldCameraY / worldShape->sectorSize + 0.5f),
                   (int32_t)(-sectorOffsetY),
                   (int32_t)(worldShape->numSectorY - 1) - sectorOffsetY);
    sectorOffsetX += deltaOffsX;
    sectorOffsetY += deltaOffsY;
    worldCameraX =
        std::clamp(worldCameraX - deltaOffsX * worldShape->sectorSize,
                   -worldShape->sectorSize / 2.0f,
                   worldShape->sectorSize / 2.0f);
    worldCameraY =
        std::clamp(worldCameraY - deltaOffsY * worldShape->sectorSize,
                   -worldShape->sectorSize / 2.0f,
                   worldShape->sectorSize / 2.0f);
}

void RenderEngine::screenToSectorCoords(const vec2& screenPx,
                                        def::SectorCoords& sectorCoords) const
{
    if (!worldShape)
    {
        return;
    }
    const float sectorSizeHalf = worldShape->sectorSize / 2.0f;
    vec2 worldPos = screenToWorldPixel(screenPx);
    int xOffset = (int)floorf(worldPos.x / worldShape->sectorSize + 0.5f);
    sectorCoords.pos.x = std::clamp(
        xOffset + sectorOffsetX, 0, (int32_t)(worldShape->numSectorX - 1));
    int yOffset = (int)floorf(worldPos.y / worldShape->sectorSize + 0.5f);
    sectorCoords.pos.y = std::clamp(
        yOffset + sectorOffsetY, 0, (int32_t)(worldShape->numSectorY - 1));

    const float S = worldShape->sectorSize;
    // worldPos is in camera-local space (same as worldCamera* after rebasing).
    // Absolute sector sx has world center sx*S in absolute coords; in local
    // space that center is (sx - sectorOffset) * S — see Sector / worldToLocal
    // style math.
    const float cx = (float(sectorCoords.pos.x) - float(sectorOffsetX)) * S;
    const float cy = (float(sectorCoords.pos.y) - float(sectorOffsetY)) * S;
    sectorCoords.sectorPos = {
        std::clamp(worldPos.x - cx, -sectorSizeHalf, sectorSizeHalf),
        std::clamp(worldPos.y - cy, -sectorSizeHalf, sectorSizeHalf)};
}

void RenderEngine::getViewportRect(Rect& rect) const
{
    const vec2 tl = screenToWorldRel(vec2(-1.0f, 1.0f));
    const vec2 br = screenToWorldRel(vec2(1.0f, -1.0f));
    rect.x = tl.x;
    rect.y = tl.y;
    rect.z = br.x;
    rect.w = br.y;
}

TextureHandle RenderEngine::getTextureHandle(const std::string& name)
{
    return textureLoader.getTextureHandle(name);
}

void RenderEngine::panWorldTo(const def::SectorCoords& sectorCoords)
{
    sectorOffsetX = sectorCoords.pos.x;
    sectorOffsetY = sectorCoords.pos.y;
    worldCameraX = sectorCoords.sectorPos.x;
    worldCameraY = sectorCoords.sectorPos.y;
}

}  // namespace gfx
// Explicitly instantiate ItemLib<Geometry> to ensure Handle is fully defined
// Must be outside the gfx namespace
template class con::ItemLib<gfx::Geometry>;