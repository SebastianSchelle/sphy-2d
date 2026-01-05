#include "texture.hpp"
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/bx.h>
#include <bx/readerwriter.h>

namespace gfx
{

Texture::Texture(const std::string& name,
                 const std::string& path,
                 const TextureIdentifier& texIdent,
                 const StoragePtr& storagePtr,
                 int atlasId)
    : name(name), path(path), texIdent(texIdent), storagePtr(storagePtr),
      atlasId(atlasId)
{
}

Texture::~Texture() {}

TextureAtlas::TextureAtlas(uint16_t width,
                           uint16_t height,
                           int bucketSize,
                           TextureIdentifier texIdent,
                           float excessHeightThreshold)
    : shelfAllocator(width, height, bucketSize, excessHeightThreshold)
{
    this->width = width;
    this->height = height;
    this->texIdent = texIdent;
}

TextureAtlas::~TextureAtlas() {}

bool TextureAtlas::insertTexture(StoragePtr& storagePtr)
{
    if (shelfAllocator.insertRect(storagePtr))
    {
        return true;
    }
    return false;
}

const TextureIdentifier TextureAtlas::getTexIdent() const
{
    return texIdent;
}

TextureArray::TextureArray(uint16_t width, uint16_t height, uint8_t layerCnt)
{
    this->width = width;
    this->height = height;
    this->layerCnt = 0;
    this->maxLayerCnt = layerCnt;
    this->handle = BGFX_INVALID_HANDLE;
}

TextureArray::~TextureArray() {}

bool TextureArray::init()
{
    // Do not overwrite existing texture array
    if (!bgfx::isValid(handle))
    {
        handle = bgfx::createTexture2D((uint16_t)width,
                                       (uint16_t)height,
                                       false,  // No mips for now
                                       (uint16_t)maxLayerCnt,
                                       bgfx::TextureFormat::BGRA8,
                                       BGFX_TEXTURE_NONE);
        return bgfx::isValid(handle);
    }
    return -1;
}

TextureIdentifier TextureArray::getFreeTexture()
{
    if (layerCnt < maxLayerCnt)
    {
        layerCnt++;
        return TextureIdentifier{handle, static_cast<uint8_t>(layerCnt - 1)};
    }
    LG_D("Texture {} has no more free layers", handle.idx);
    return TextureIdentifier{INVALID_TEX_HANDLE, 0};
}

TextureLoader::TextureLoader() {
}

void TextureLoader::init(int texWidth,
                         int texHeight,
                         int texLayerCnt,
                         int bucketSize,
                         float excessHeightThreshold)
{
    this->texLayerCnt = texLayerCnt;
    this->texWidth = texWidth;
    this->texHeight = texHeight;
    this->bucketSize = bucketSize;
    this->excessHeightThreshold = excessHeightThreshold;
}

TextureLoader::~TextureLoader() {}

void TextureLoader::unloadTexture(uint32_t handle) {}


uint32_t TextureLoader::loadTexture(const std::string& name,
                                    const std::string& type,
                                    const std::string& path)
{
    bx::Error err;
    // Load image file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LG_E("Failed to open file: {}", path);
        return 0;
    }

    auto sizef = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // Read file into buffer
    std::vector<uint8_t> buffer(sizef);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), sizef))
    {
        LG_E("Failed to read file {}", path);
        return 0;
    }
    bx::DefaultAllocator alloc;
    bimg::ImageContainer* image =
        bimg::imageParseDds(&alloc, buffer.data(), sizef, &err);
    if (!image)
    {
        LG_E("Failed to parse image: {}", path)
        return 0;
    }

    LG_D("Read image file {} successfully", path);
    LG_D("alpha {}", image->m_hasAlpha);
    LG_D("width {}", image->m_width);
    LG_D("height {}", image->m_height);
    LG_D("format {}", (int)image->m_format);
    LG_D("size {}", image->m_size);

    if (!err.isOk() || image->m_data == nullptr)
    {
        LG_E("Failed to parse image: {}", path)
        bimg::imageFree(image);
        return 0;
    }


    StoragePtr storagePtr;
    storagePtr.rect.width = image->m_width;
    storagePtr.rect.height = image->m_height;
    uint32_t texId = insertIntoAtlas(
        name, path, type, image->m_width, image->m_height, storagePtr, image->m_data);

    if (texId != 0)
    {
        return texId;
    }
    else
    {
        int atlasIdx = createNewAtlas(type);
        if (atlasIdx != -1)
        {
            TextureAtlas* atlas = textureAtlasLib.getItem(atlasIdx);
            if (atlas)
            {
                if (atlas->insertTexture(storagePtr))
                {
                    return makeTexture(
                        name, path, storagePtr, atlas->getTexIdent(), atlasIdx, image->m_data);
                }
            }
        }
    }

    LG_E("Failed to store texture {} with path {} in GPU memory", name, path);
    return 0;
}

uint32_t TextureLoader::insertIntoAtlas(const std::string& name,
                                        const std::string& path,
                                        const std::string& type,
                                        int width,
                                        int height,
                                        StoragePtr& storagePtr,
                                        void* rgbaData)
{
    auto it = atlasRegistry.find(type);
    if (it != atlasRegistry.end())
    {
        auto reg = it->second;
        for (auto& entry : reg)
        {
            TextureAtlas* atlas = textureAtlasLib.getItem(entry);
            if (atlas)
            {
                if (atlas->insertTexture(storagePtr))
                {
                    return makeTexture(
                        name, path, storagePtr, atlas->getTexIdent(), entry, rgbaData);
                }
            }
        }
    }
    return false;
}

uint32_t TextureLoader::makeTexture(const std::string& name,
                                    const std::string& path,
                                    StoragePtr& storagePtr,
                                    TextureIdentifier texIdent,
                                    int atlasId,
                                    void* rgbaData)
{
    // Update texture in VRAM
    const bgfx::Memory* mem = bgfx::copy(
        rgbaData, storagePtr.rect.width * storagePtr.rect.height * 4);

    bgfx::updateTexture2D(texIdent.texHandle,
                          texIdent.layerIdx,
                          0,
                          storagePtr.rect.x,
                          storagePtr.rect.y,
                          storagePtr.rect.width,
                          storagePtr.rect.height,
                          mem);

    Texture texture(name, path, texIdent, storagePtr, atlasId);
    int idx = textureLib.addItem(name, texture);
    uint32_t wrapped = textureLib.wrappedIdx(idx);
    LG_I("Texture has been added to GPU storage");
    LG_I("GPU Texture handle: {}, Layer: {}",
         texture.getTexIdent().texHandle.idx,
         texture.getTexIdent().layerIdx);
    LG_I("Texture name: {}, Path: {}, Lib idx: {}, Lib size: {}", 
         name, path, idx, textureLib.size());
    LG_I("Atlas ID: {}, Pos: ({},{}) - {}x{}",
         atlasId,
         texture.getStoragePtr().rect.x,
         texture.getStoragePtr().rect.y,
         texture.getStoragePtr().rect.width,
         texture.getStoragePtr().rect.height);
    LG_I("Wrapped texture handle: {} (idx: {}, gen: {})", 
         wrapped, idx, textureLib.getGeneration(idx));
    return wrapped;
}

int TextureLoader::createNewAtlas(const std::string& type)
{
    // Check if a free layer is available in any texture array
    for (auto& textureArray : textureArrays)
    {
        auto texIdent = textureArray.getFreeTexture();
        LG_D("Found free texture array layer {} in texture array {}",
             texIdent.layerIdx,
             texIdent.texHandle.idx);
        if (bgfx::isValid(texIdent.texHandle))
        {
            con::IdxUuid idxUuid = textureAtlasLib.addWithRandomKey(
                TextureAtlas(texWidth,
                             texHeight,
                             bucketSize,
                             texIdent,
                             excessHeightThreshold));
            LG_D("Created new texture atlas with id {} for type {}",
                 idxUuid.idx,
                 type);
            if (idxUuid.idx != -1)
            {
                atlasRegistry[type].push_back(idxUuid.idx);
                return idxUuid.idx;
            }
            LG_D("Something went wrong creating new texture atlas");
            return -1;
        }
    }
    LG_D("No free texture array layer found. Create new texture array...",
         type);
    TextureArray textureArray(texWidth, texHeight, texLayerCnt);
    if (!textureArray.init())
    {
        LG_E("Failed to create new texture array");
        return -1;
    }
    textureArrays.push_back(textureArray);
    return createNewAtlas(type);
}

con::ItemLib<Texture>& TextureLoader::getTextureLib()
{
    return textureLib;
}

}  // namespace gfx
