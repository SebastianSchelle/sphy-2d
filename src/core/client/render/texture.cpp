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
                 TextureAtlasHandle atlasHandle)
    : name(name), path(path), texIdent(texIdent), storagePtr(storagePtr),
      atlasHandle(atlasHandle)
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

TextureLoader::TextureLoader() {}

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


TextureHandle TextureLoader::loadTexture(const std::string& name,
                                         const std::string& type,
                                         const std::string& path)
{
    bx::Error err;
    // Load image file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LG_E("Failed to open file: {}", path);
        return TextureHandle::Invalid();
    }

    auto sizef = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // Read file into buffer
    std::vector<uint8_t> buffer(sizef);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), sizef))
    {
        LG_E("Failed to read file {}", path);
        return TextureHandle::Invalid();
    }
    bx::DefaultAllocator alloc;
    bimg::ImageContainer* image =
        bimg::imageParseDds(&alloc, buffer.data(), sizef, &err);
    if (!image)
    {
        LG_E("Failed to parse image: {}", path)
        return TextureHandle::Invalid();
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
        return TextureHandle::Invalid();
    }


    StoragePtr storagePtr;
    storagePtr.rect.width = image->m_width;
    storagePtr.rect.height = image->m_height;
    TextureHandle handle = insertIntoAtlas(name,
                                           path,
                                           type,
                                           image->m_width,
                                           image->m_height,
                                           storagePtr,
                                           image->m_data);

    if (handle.isValid())
    {
        return handle;
    }
    else
    {
        TextureAtlasHandle atlasHandle = createNewAtlas(type);
        if (atlasHandle.isValid())
        {
            TextureAtlas* atlas = textureAtlasLib.getItem(atlasHandle);
            if (atlas)
            {
                if (atlas->insertTexture(storagePtr))
                {
                    return makeTexture(name,
                                       path,
                                       storagePtr,
                                       atlas->getTexIdent(),
                                       atlasHandle,
                                       image->m_data);
                }
            }
        }
    }

    LG_E("Failed to store texture {} with path {} in GPU memory", name, path);
    return TextureHandle::Invalid();
}

TextureHandle TextureLoader::insertIntoAtlas(const std::string& name,
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
                    TextureAtlasHandle atlasHandle = textureAtlasLib.getHandle(entry);
                    return makeTexture(name,
                                       path,
                                       storagePtr,
                                       atlas->getTexIdent(),
                                       atlasHandle,
                                       rgbaData);
                }
            }
        }
    }
    return TextureHandle::Invalid();
}

TextureHandle TextureLoader::makeTexture(const std::string& name,
                                         const std::string& path,
                                         StoragePtr& storagePtr,
                                         TextureIdentifier texIdent,
                                         TextureAtlasHandle atlasHandle,
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

    Texture texture(name, path, texIdent, storagePtr, atlasHandle);
    int idx = textureLib.addItem(name, texture);
    TextureHandle handle = textureLib.getHandle(idx);
    LG_I("Texture has been added to GPU storage");
    LG_I("GPU Texture handle: {}, Layer: {}",
         texture.getTexIdent().texHandle.idx,
         texture.getTexIdent().layerIdx);
    LG_I("Texture name: {}, Path: {}, Lib idx: {}, Lib size: {}",
         name,
         path,
         idx,
         textureLib.size());
    LG_I("Atlas ID: {}, Pos: ({},{}) - {}x{}",
         atlasHandle.value(),
         texture.getStoragePtr().rect.x,
         texture.getStoragePtr().rect.y,
         texture.getStoragePtr().rect.width,
         texture.getStoragePtr().rect.height);
    LG_I("Wrapped texture handle: {} (idx: {}, gen: {})",
         handle.value(),
         idx,
         textureLib.getGeneration(idx));
    return handle;
}

TextureAtlasHandle TextureLoader::createNewAtlas(const std::string& type)
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
                return textureAtlasLib.getHandle(idxUuid.idx);
            }
            LG_D("Something went wrong creating new texture atlas");
            return TextureAtlasHandle::Invalid();
        }
    }
    LG_D("No free texture array layer found. Create new texture array...",
         type);
    TextureArray textureArray(texWidth, texHeight, texLayerCnt);
    if (!textureArray.init())
    {
        LG_E("Failed to create new texture array");
        return TextureAtlasHandle::Invalid();
    }
    textureArrays.push_back(textureArray);
    TextureAtlasHandle handle = createNewAtlas(type);
    return handle;
}

con::ItemLib<Texture>& TextureLoader::getTextureLib()
{
    return textureLib;
}

}  // namespace gfx

// Explicitly instantiate ItemLib<T> to ensure Handle is fully defined
// Must be outside the gfx namespace
template class con::ItemLib<gfx::TextureAtlas>;
template class con::ItemLib<gfx::Texture>;