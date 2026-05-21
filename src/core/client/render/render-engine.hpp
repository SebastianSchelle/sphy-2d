#ifndef RENDER_ENGINE_HPP
#define RENDER_ENGINE_HPP

#include "std-inc.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <config-manager.hpp>
#include <item-lib.hpp>
#include <magic_enum/magic_enum.hpp>
#include <shader.hpp>
#include <texture.hpp>
#include <vertex-defines.hpp>
#include <world-def.hpp>

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

enum class GameViewMode
{
    ThirdPerson,
    TacticalMap,
    StrategicMap,
    Menu,
    AtlasDebug,
    ModdingTools,
    Count,
};

struct PersistentCamPos
{
    float x;
    float y;
    float xOffs;
    float yOffs;
    float zoom;
};

struct Geometry
{
  public:
    Geometry(const void* vertexData,
             size_t vDatSize,
             const void* indexData,
             size_t iDatSize,
             bgfx::VertexLayout& vertLayout,
             bool use32BitIndices = false);

    bgfx::VertexBufferHandle getVertexBufferHandle() const;
    bgfx::IndexBufferHandle getIndexBufferHandle() const;
    void destroy();

  private:
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
};

using GeometryHandle = typename con::ItemLib<Geometry>::Handle;
using GeometryHandleUuid = typename con::ItemLib<Geometry>::HandleUuid;

struct TexRectData
{
    vec4 rect;
    vec4 atlasUv;
    vec4 rotLay;
    vec4 uvOffScale;
    vec4 colorAbgr;
};

struct ZoomPanCfg
{
    float zoomStep;
    float maxZoom;
    float minZoom;
    float panSpeed;
};

enum class PanDirection
{
    Up = -1,
    Down = 1,
    Left = -1,
    Right = 1,
    Stop = 0,
};

struct ZSortEntry
{
    uint32_t vecIdx;
    int8_t zIndex;
    uint16_t texArrayIdx;
};

struct TexRectDataWrapper
{
    bgfx::TextureHandle arrayHandle;
    TexRectData texRectData;
    bgfx::ViewId viewId;
};

class RenderEngine
{
  public:
    constexpr static const int8_t zIdxMapIconHull = 0;
    constexpr static const int8_t zIdxMapIconStation = 100;

    constexpr static const int8_t zIdxShipHull = 0;
    constexpr static const int8_t zIdxStation = 50;
    constexpr static const int8_t zIdxDrone = 100;

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

    RenderEngine(const RenderEngine&) = delete;
    RenderEngine& operator=(const RenderEngine&) = delete;
    RenderEngine(RenderEngine&&) = delete;
    RenderEngine& operator=(RenderEngine&&) = delete;

    bool initPre();
    bool initPost();
    void shutdown();

    void startFrame();
    void endFrame();

    void setWindowSize(int width, int height);
    glm::ivec2 getWindowPixelSize() const
    {
        return {winWidth, winHeight};
    }

    void setScissorRegion(const glm::vec2& position, const glm::vec2& size);
    void setScissorRegionEnabled(bool enabled);
    void setTransform(const glm::mat4& transform);

    void zoom(float amount);
    void panWorld(PanDirection dirX, PanDirection dirY);
    void panWorld(const glm::vec2& delta);
    void panWorldTo(const def::SectorCoords& sectorCoords);
    void setActiveSector(int32_t sectorX, int32_t sectorY);
    void clbToggleTacticalView();
    void clbToggleStrategicView();
    void gotoModdingTools();
    void startGame();

    void setWorldShape(const def::WorldShape* worldShape);
    void updateWorldView();

    GameViewMode getViewMode() const
    {
        return viewMode;
    }
    glm::vec2 getWorldCameraPosition() const
    {
        return {worldCameraX, worldCameraY};
    }
    float getWorldZoom() const
    {
        return worldZoom;
    }
    int32_t getSectorOffsetX() const
    {
        return sectorOffsetX;
    }
    int32_t getSectorOffsetY() const
    {
        return sectorOffsetY;
    }

    vec2 screenToWorldPixel(const vec2& screenPx) const;
    vec2 screenToWorldRel(const vec2& screenPosRel) const;
    void screenToSectorCoords(const glm::vec2& screenPx,
                              def::SectorCoords& sectorCoords) const;
    void getViewportRect(smath::Rect& rect) const;

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
    TextureHandle getTextureHandle(const std::string& name);
    bool getTexturePixelSize(const std::string& name, glm::vec2& sizePx);
    bool getTextureFilePath(const std::string& name, std::string& pathOut);
    std::vector<std::string> getTextureNames() const;
    TextureHandle getFallbackTextureHandle() const
    {
        return textureHandleFallback;
    }
    glm::ivec2 getAtlasTextureSize() const;
    size_t getGpuTextureArrayCount() const;
    bool getGpuTextureArrayInfo(size_t index, GpuTextureArrayInfo& out) const;
    std::string getAtlasRegistrySummary() const;
    void fillAtlasDebugGpuArrayOptions(
        std::vector<AtlasDebugSelectOption>& out) const;
    void fillAtlasDebugLayerOptions(
        int gpuArrayIndex,
        std::vector<AtlasDebugSelectOption>& out) const;
    void fillAtlasDebugMipOptions(int gpuArrayIndex,
                                 std::vector<AtlasDebugSelectOption>& out) const;
    void fillAtlasDebugKindPickRows(std::vector<AtlasDebugKindPickRow>& out);

    ShaderHandle loadShader(const std::string& name,
                            const std::string& vsPath,
                            const std::string& fsPath);
    void releaseShader(ShaderHandle handle);
    ShaderHandle getShaderHandle(const std::string& name) const;

    GeometryHandle compileGeometry(const void* vertexData,
                                   size_t vDatSize,
                                   const void* indexData,
                                   size_t iDatSize,
                                   bgfx::VertexLayout& vertLayout,
                                   bool use32BitIndices = false);
    void releaseGeometry(GeometryHandle handle);
    void renderCompiledGeometry(GeometryHandle geometryHandle,
                                const glm::vec2& translation,
                                TextureHandle textureHandle,
                                bgfx::ViewId viewId = 0);

    void drawTexturedQuad(const glm::vec2& translation,
                          float rotation,
                          TextureHandle textureHandle,
                          bgfx::ViewId viewId = 0);
    void queueTexRect(const glm::vec2& pos,
                      const glm::vec2& size,
                      TextureHandle textureHandle,
                      float rotationRad,
                      int8_t zIndex,
                      uint32_t colorABGR = 0xffffffff,
                      bgfx::ViewId viewId = 0,
                      const glm::vec2& uvOffset = glm::vec2(0.0f),
                      const glm::vec2& uvScale = glm::vec2(1.0f));
    void flushQueuedTexRects();
    void drawShapeRectangle(const glm::vec2& pos,
                            const glm::vec2& size,
                            uint32_t colorABGR,
                            float thickness,
                            float rotationRad = 0.0f,
                            float zIndex = 0.0f,
                            bgfx::ViewId viewId = 0);
    void drawEllipse(const glm::vec2& pos,
                     const glm::vec2& size,
                     uint32_t colorABGR,
                     float thickness,
                     float rotationRad = 0.0f,
                     float zIndex = 0.0f,
                     bgfx::ViewId viewId = 0);
    void drawLine(const glm::vec2& start,
                  const glm::vec2& end,
                  uint32_t colorABGR,
                  float thickness,
                  float zIndex = 0.0f,
                  bgfx::ViewId viewId = 0);
    void drawFullScreenTriangles(bgfx::ViewId viewId,
                                 ShaderHandle shaderHandle);
    void drawBlueprintGridBackground(bgfx::ViewId viewId,
                                     ShaderHandle shaderHandle,
                                     float cellWorld,
                                     float majorEveryCells = 5.0f);
    void drawDebugCheckerboard(bgfx::ViewId viewId,
                               ShaderHandle shaderHandle,
                               float cellWorld = 1000.0f,
                               float highlightStrength = 0.08f);
    void drawAtlasDebugLayer(bgfx::ViewId viewId,
                             bgfx::TextureHandle texArray,
                             uint8_t layer,
                             uint8_t mipLevel,
                             uint16_t texWidthFull,
                             uint16_t texHeightFull,
                             int previewX,
                             int previewY,
                             int previewW,
                             int previewH);

    tim::Timepoint getStartTime() const;

  private:
    void cleanupAll();
    void cleanupTextures();
    void cleanupShaders();
    void cleanupGeometry();
    void updateOrtho();

    void changeRenderState(RenderState newState);
    void allocateForShapes();
    void submitShapes();
    void allocateForTexRects();
    void submitTexRects();
    void flushQueuedTexRect();

    void enqueueShape(float shapeType,
                      const glm::vec2& pos,
                      const glm::vec2& size,
                      uint32_t colorABGR,
                      float thickness,
                      float rotationRad = 0.0f,
                      float zIndex = 0.0f,
                      bgfx::ViewId viewId = 0);
    void applyCameraSectorRebase();
    void saveViewCameraState(GameViewMode mode);
    void restoreViewCameraState(GameViewMode mode);

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
    bgfx::UniformHandle u_atlasDbgView = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_atlasPos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_time = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_grid = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_transform = BGFX_INVALID_HANDLE;

    ShaderHandle shaderHandleRml = ShaderHandle::Invalid();
    ShaderHandle shaderHandleShapes = ShaderHandle::Invalid();
    ShaderHandle shaderHandleTexRect = ShaderHandle::Invalid();
    TextureHandle textureHandleFallback = TextureHandle::Invalid();

    tim::Timepoint startTime;
    float frameTime;

    int winWidth;
    int winHeight;
    float ortho[16];
    float worldCameraX = 0.0f;
    float worldCameraY = 0.0f;
    float worldZoom = 1.0f;
    float worldView[16];
    float worldViewProj[16];
    float invWvp[16];

    ZoomPanCfg camMoveCfg[static_cast<size_t>(GameViewMode::Count)];
    PersistentCamPos persistentCamPos[static_cast<size_t>(GameViewMode::Count)];

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
    bool hasShutdown = false;
    const def::WorldShape* worldShape = nullptr;
    int32_t sectorOffsetX = 0;
    int32_t sectorOffsetY = 0;
    int32_t activeSectorX = 0;
    int32_t activeSectorY = 0;

    bgfx::InstanceDataBuffer idbTex;
    size_t maxTexPerDrawCall = 1024;
    size_t currentTexRectCount = 0;
    bgfx::TextureHandle texRectBatchArray = BGFX_INVALID_HANDLE;
    vector<TexRectDataWrapper> texRectData;
    vector<ZSortEntry> texRectSorted;

    GameViewMode viewMode = GameViewMode::ThirdPerson;
};

}  // namespace gfx

EXT_FMT(gfx::GameViewMode, "{}", magic_enum::enum_name(o));

#endif
