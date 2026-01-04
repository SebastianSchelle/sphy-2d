#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "item-lib.hpp"
#include <rectpack2D/rect_structs.h>
#include <shelf-allocator.hpp>
#include <bgfx/bgfx.h>

namespace gfx
{

#define INVALID_TEX_HANDLE 0xFFFF

using StoragePtr = con::alloc::StoragePtr;

struct TextureIdentifier
{
    bgfx::TextureHandle texHandle;
    uint8_t layerIdx;
};

class Texture
{
  public:
    Texture(const std::string& name,
            const std::string& path,
            const TextureIdentifier& texIdent,
            const StoragePtr& storagePtr,
            int atlasId);
    ~Texture();
    const TextureIdentifier getTexIdent() const { return texIdent; }
    const std::string getName() const { return name; }
    const std::string getPath() const { return path; }
    const int getAtlasId() const { return atlasId; }
    const StoragePtr getStoragePtr() const { return storagePtr; }

  private:
    std::string name;
    std::string path;
    TextureIdentifier texIdent;
    StoragePtr storagePtr;
    int atlasId;
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
    uint32_t getHandle() const;
    bool insertTexture(StoragePtr& storagePtr);
    const TextureIdentifier getTexIdent() const;

  private:
    TextureIdentifier texIdent;
    uint16_t width;
    uint16_t height;
    con::alloc::ShelfAllocator shelfAllocator;
};

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
    uint32_t loadTexture(const std::string& name,
                         const std::string& type,
                         const std::string& path);
    void unloadTexture(uint32_t handle);

  private:
    uint32_t insertIntoAtlas(const std::string& name,
                             const std::string& path,
                             const std::string& type,
                             int width,
                             int height,
                             StoragePtr& storagePtr);
    int createNewAtlas(const std::string& type);
    uint32_t makeTexture(const std::string& name,
                         const std::string& path,
                         StoragePtr& storagePtr,
                         TextureIdentifier texIdent,
                         int atlasId);
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