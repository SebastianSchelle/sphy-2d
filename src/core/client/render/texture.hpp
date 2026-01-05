#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "item-lib.hpp"
#include <bgfx/bgfx.h>
#include <rectpack2D/rect_structs.h>
#include <shelf-allocator.hpp>

namespace gfx
{

#define INVALID_TEX_HANDLE 0xFFFF

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

// Type alias for TextureAtlas handle - must use typename for nested template
// type
using TextureAtlasHandle = typename con::ItemLib<TextureAtlas>::Handle;

struct UvRect
{
  public:
    UvRect(float xMin, float yMin, float xMax, float yMax);
    float xMin;
    float yMin;
    float xMax;
    float yMax;
};

class Texture
{
  public:
    Texture(const std::string& name,
            const std::string& path,
            const TextureIdentifier& texIdent,
            const StoragePtr& storagePtr,
            TextureAtlasHandle atlasHandle,
            const UvRect& uvRect);
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
    const UvRect& getUvRect() const
    {
        return uvRect;
    }

  private:
    std::string name;
    std::string path;
    TextureIdentifier texIdent;
    StoragePtr storagePtr;
    TextureAtlasHandle atlasHandle;
    UvRect uvRect;
};

using TextureHandle = typename con::ItemLib<Texture>::Handle;

class TextureArray
{
  public:
    TextureArray(uint16_t width, uint16_t height, uint8_t layerCnt);
    ~TextureArray();
    bool init();
    TextureIdentifier getFreeTexture();

  private:
    bgfx::TextureHandle handle;
    uint16_t width;
    uint16_t height;
    uint8_t layerCnt;
    uint8_t maxLayerCnt;
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
    void unloadTexture(uint32_t handle);
    con::ItemLib<Texture>& getTextureLib();

  private:
    TextureHandle insertIntoAtlas(const std::string& name,
                                  const std::string& path,
                                  const std::string& type,
                                  int width,
                                  int height,
                                  StoragePtr& storagePtr,
                                  void* rgbaData);
    TextureAtlasHandle createNewAtlas(const std::string& type);
    TextureHandle makeTexture(const std::string& name,
                              const std::string& path,
                              StoragePtr& storagePtr,
                              TextureIdentifier texIdent,
                              TextureAtlasHandle atlasHandle,
                              void* rgbaData);
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