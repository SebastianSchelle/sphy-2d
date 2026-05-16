#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "item-lib.hpp"
#include <bgfx/bgfx.h>
#include <rectpack2D/rect_structs.h>
#include <shelf-allocator.hpp>

namespace gfx
{

#define INVALID_TEX_HANDLE 0xFFFF

/// RmlUI glyph sheets (GenerateTexture): no mips, point-sampled when drawn.
inline constexpr const char* kAtlasTypeRmlUiFont = "rmlui-font";
/// RmlUI images from files (LoadTexture): full mip chain + linear sampling.
inline constexpr const char* kAtlasTypeRmlUi = "rmlui";

using StoragePtr = con::alloc::StoragePtr;

struct TextureIdentifier
{
    bgfx::TextureHandle texHandle;
    uint8_t layerIdx;
};

class TextureAtlas
{
  public:
    TextureAtlas(uint16_t width,
                 uint16_t height,
                 int bucketSize,
                 TextureIdentifier texIdent,
                 float excessHeightThreshold);
    ~TextureAtlas();
    bool insertTexture(StoragePtr& storagePtr);
    const TextureIdentifier getTexIdent() const;

  private:
    TextureIdentifier texIdent;
    uint16_t width;
    uint16_t height;
    con::alloc::ShelfAllocator shelfAllocator;
};

using TextureAtlasHandle = typename con::ItemLib<TextureAtlas>::Handle;
using TextureAtlasHandleUuid = typename con::ItemLib<TextureAtlas>::HandleUuid;

class Texture
{
  public:
    Texture(const std::string& name,
            const std::string& path,
            const TextureIdentifier& texIdent,
            const StoragePtr& storagePtr,
            TextureAtlasHandle atlasHandle,
            glm::vec4 relBounds,
            bool pointSample = false);
    ~Texture();
    const TextureIdentifier getTexIdent() const
    {
        return texIdent;
    }
    const std::string getName() const
    {
        return name;
    }
    const std::string getPath() const
    {
        return path;
    }
    const TextureAtlasHandle getAtlasHandle() const
    {
        return atlasHandle;
    };
    const StoragePtr getStoragePtr() const
    {
        return storagePtr;
    }
    const glm::vec4 getRelBounds() const
    {
        return relBounds;
    }
    bool usesPointSampling() const
    {
        return pointSample;
    }

  private:
    std::string name;
    std::string path;
    TextureIdentifier texIdent;
    StoragePtr storagePtr;
    TextureAtlasHandle atlasHandle;
    glm::vec4 relBounds;
    bool pointSample = false;
};

using TextureHandle = typename con::ItemLib<Texture>::Handle;

/// One GPU texture2DArray backing packed atlases (debug / inspector).
struct GpuTextureArrayInfo
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t layersUsed = 0;
    uint8_t layersCapacity = 0;
    bool hasMipmaps = true;
};

/// Label + integer value for RmlUi &lt;select data-value&gt; options (atlas debug).
struct AtlasDebugSelectOption
{
    int value = 0;
    std::string label;
};

/// One row per `atlasRegistry` type (first atlas page) for debug UI routing.
struct AtlasDebugKindPickRow
{
    std::string label;
    int gpuArrayIndex = -1;
    int layerIndex = 0;
};

class TextureArray
{
  public:
    TextureArray(uint16_t width, uint16_t height, uint8_t layerCnt, bool generateMipmaps);
    ~TextureArray();
    bool init();
    TextureIdentifier getFreeTexture();
    void getGpuInfo(GpuTextureArrayInfo& out) const;
    bool hasMipmaps() const
    {
        return generateMipmaps;
    }

  private:
    bgfx::TextureHandle handle;
    uint16_t width;
    uint16_t height;
    uint8_t layerCnt;
    uint8_t maxLayerCnt;
    bool generateMipmaps;
};

class TextureLoader
{
  public:
    TextureLoader();
    ~TextureLoader();
    void init(int texWidth,
              int texHeight,
              int texLayerCnt,
              int bucketSize,
              float excessHeightThreshold);
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
    void unloadTexture(uint32_t handle);
    con::ItemLib<Texture>& getTextureLib();
    std::vector<std::string> getTextureNames() const;
    TextureHandle getTextureHandle(const std::string& name);
    size_t getGpuTextureArrayCount() const;
    bool getGpuTextureArrayInfo(size_t index, GpuTextureArrayInfo& out) const;
    /// Sorted `atlasRegistry` keys (comma-separated) for debug UI.
    std::string getAtlasRegistrySummary() const;
    void fillAtlasDebugGpuArrayOptions(std::vector<AtlasDebugSelectOption>& out) const;
    void fillAtlasDebugLayerOptions(int gpuArrayIndex,
                                   std::vector<AtlasDebugSelectOption>& out) const;
    void fillAtlasDebugMipOptions(int gpuArrayIndex,
                                  std::vector<AtlasDebugSelectOption>& out) const;
    void fillAtlasDebugKindPickRows(std::vector<AtlasDebugKindPickRow>& out);

  private:
    TextureHandle insertIntoAtlas(const std::string& name,
                                  const std::string& path,
                                  const std::string& type,
                                  int width,
                                  int height,
                                  StoragePtr& storagePtr,
                                  const void* rgbaData);
    TextureAtlasHandle createNewAtlas(const std::string& type);
    TextureHandle makeTexture(const std::string& name,
                              const std::string& path,
                              StoragePtr& storagePtr,
                              TextureIdentifier texIdent,
                              TextureAtlasHandle atlasHandle,
                              const void* rgbaData,
                              bool fontGlyph);
    static bool isFontAtlasType(const std::string& type);
    int gpuArrayIndexOfHandle(bgfx::TextureHandle h) const;

    std::unordered_map<std::string, std::vector<int>> atlasRegistry;
    con::ItemLib<TextureAtlas> textureAtlasLib;
    con::ItemLib<Texture> textureLib;
    std::vector<TextureArray> textureArrays;
    uint8_t texLayerCnt;
    uint16_t texWidth;
    uint16_t texHeight;
    int bucketSize;
    float excessHeightThreshold;
};

}  // namespace gfx

#endif