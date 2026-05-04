#include "texture.hpp"
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/bx.h>
#include <bx/readerwriter.h>
#include <vector>

namespace gfx
{

constexpr uint16_t kTexturePadding = 8;

namespace
{
struct MipLevelData
{
    std::vector<uint8_t> pixels;
    uint16_t width;
    uint16_t height;
};

std::vector<uint8_t> makePaddedImage(const uint8_t* rgbaData,
                                     uint16_t srcWidth,
                                     uint16_t srcHeight,
                                     uint16_t padding)
{
    const uint16_t paddedWidth = srcWidth + padding * 2;
    const uint16_t paddedHeight = srcHeight + padding * 2;
    std::vector<uint8_t> padded(paddedWidth * paddedHeight * 4);

    for (uint16_t y = 0; y < paddedHeight; ++y)
    {
        const uint16_t srcY = (y < padding)
                                  ? 0
                                  : (y >= padding + srcHeight ? srcHeight - 1
                                                              : y - padding);
        for (uint16_t x = 0; x < paddedWidth; ++x)
        {
            const uint16_t srcX =
                (x < padding)
                    ? 0
                    : (x >= padding + srcWidth ? srcWidth - 1 : x - padding);

            const uint32_t srcIdx = (srcY * srcWidth + srcX) * 4;
            const uint32_t dstIdx = (y * paddedWidth + x) * 4;
            padded[dstIdx + 0] = rgbaData[srcIdx + 0];
            padded[dstIdx + 1] = rgbaData[srcIdx + 1];
            padded[dstIdx + 2] = rgbaData[srcIdx + 2];
            padded[dstIdx + 3] = rgbaData[srcIdx + 3];
        }
    }

    return padded;
}

std::vector<MipLevelData> buildMipChain(const uint8_t* rgbaData,
                                        uint16_t width,
                                        uint16_t height)
{
    std::vector<MipLevelData> chain;
    chain.push_back({std::vector<uint8_t>(rgbaData, rgbaData + width * height * 4),
                     width,
                     height});

    while (chain.back().width > 1 || chain.back().height > 1)
    {
        const MipLevelData& src = chain.back();
        const uint16_t dstWidth = std::max<uint16_t>(1, src.width / 2);
        const uint16_t dstHeight = std::max<uint16_t>(1, src.height / 2);
        std::vector<uint8_t> dstPixels(dstWidth * dstHeight * 4);

        for (uint16_t y = 0; y < dstHeight; ++y)
        {
            for (uint16_t x = 0; x < dstWidth; ++x)
            {
                const uint16_t sx0 = std::min<uint16_t>(src.width - 1, uint16_t(x * 2));
                const uint16_t sy0 =
                    std::min<uint16_t>(src.height - 1, uint16_t(y * 2));
                const uint16_t sx1 =
                    std::min<uint16_t>(src.width - 1, uint16_t(sx0 + 1));
                const uint16_t sy1 =
                    std::min<uint16_t>(src.height - 1, uint16_t(sy0 + 1));

                const uint32_t i00 = (sy0 * src.width + sx0) * 4;
                const uint32_t i10 = (sy0 * src.width + sx1) * 4;
                const uint32_t i01 = (sy1 * src.width + sx0) * 4;
                const uint32_t i11 = (sy1 * src.width + sx1) * 4;
                const uint32_t di = (y * dstWidth + x) * 4;
                const uint32_t a00 = src.pixels[i00 + 3];
                const uint32_t a10 = src.pixels[i10 + 3];
                const uint32_t a01 = src.pixels[i01 + 3];
                const uint32_t a11 = src.pixels[i11 + 3];
                const uint32_t alphaSum = a00 + a10 + a01 + a11;
                const uint32_t alphaAvg = (alphaSum + 2) / 4;
                const uint32_t alphaMax = std::max(
                    std::max(a00, a10), std::max(a01, a11));

                // Alpha-weighted RGB keeps transparent edge mattes from bleeding
                // into lower mips (common source of white halos).
                for (uint8_t c = 0; c < 3; ++c)
                {
                    if (alphaSum > 0)
                    {
                        const uint32_t weighted =
                            uint32_t(src.pixels[i00 + c]) * a00 +
                            uint32_t(src.pixels[i10 + c]) * a10 +
                            uint32_t(src.pixels[i01 + c]) * a01 +
                            uint32_t(src.pixels[i11 + c]) * a11;
                        dstPixels[di + c] =
                            static_cast<uint8_t>((weighted + alphaSum / 2) / alphaSum);
                    }
                    else
                    {
                        dstPixels[di + c] = 0;
                    }
                }

                // Preserve edge coverage to avoid jagged silhouette shrink in
                // lower mips. Bias alpha slightly toward the max sample alpha.
                const uint32_t alphaCoverage =
                    alphaAvg + ((alphaMax - alphaAvg) * 3) / 8;
                dstPixels[di + 3] =
                    static_cast<uint8_t>(std::min<uint32_t>(255, alphaCoverage));
            }
        }

        chain.push_back({std::move(dstPixels), dstWidth, dstHeight});
    }

    return chain;
}
}  // namespace

Texture::Texture(const std::string& name,
                 const std::string& path,
                 const TextureIdentifier& texIdent,
                 const StoragePtr& storagePtr,
                 TextureAtlasHandle atlasHandle,
                 glm::vec4 relBounds)
    : name(name), path(path), texIdent(texIdent), storagePtr(storagePtr),
      atlasHandle(atlasHandle), relBounds(relBounds)
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
    LG_W("Failed to insert texture into atlas");
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
                                       true,
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
                                         const std::string& path,
                                         glm::vec2& dimensions)
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
        LG_E("Failed to parse image: {}", path);
        return TextureHandle::Invalid();
    }

    dimensions.x = image->m_width;
    dimensions.y = image->m_height;

    LG_D("Read image file {} successfully", path);
    LG_D("alpha {}", image->m_hasAlpha);
    LG_D("width {}", image->m_width);
    LG_D("height {}", image->m_height);
    LG_D("format {}", (int)image->m_format);
    LG_D("size {}", image->m_size);

    if (!err.isOk() || image->m_data == nullptr)
    {
        LG_E("Failed to parse image: {}", path);
        bimg::imageFree(image);
        return TextureHandle::Invalid();
    }
    return generateTexture(
        name, type, image->m_data, image->m_width, image->m_height, path);
}

TextureHandle TextureLoader::loadTexture(const std::string& name,
                                         const std::string& type,
                                         const std::string& path)
{
    glm::vec2 dimensions;
    return loadTexture(name, type, path, dimensions);
}

TextureHandle TextureLoader::generateTexture(const std::string& name,
                                             const std::string& type,
                                             const void* data,
                                             int width,
                                             int height,
                                             const std::string& path)
{
    StoragePtr storagePtr;
    storagePtr.rect.width = width + kTexturePadding * 2;
    storagePtr.rect.height = height + kTexturePadding * 2;
    TextureHandle handle =
        insertIntoAtlas(name, path, type, width, height, storagePtr, data);

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
                                       data);
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
                                             const void* rgbaData)
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
                    TextureAtlasHandle atlasHandle =
                        textureAtlasLib.getHandle(entry);
                    
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
                                         const void* rgbaData)
{
    const uint16_t outerX = storagePtr.rect.x;
    const uint16_t outerY = storagePtr.rect.y;
    const uint16_t outerWidth = storagePtr.rect.width;
    const uint16_t outerHeight = storagePtr.rect.height;

    storagePtr.rect.x += kTexturePadding;
    storagePtr.rect.y += kTexturePadding;
    storagePtr.rect.width -= kTexturePadding * 2;
    storagePtr.rect.height -= kTexturePadding * 2;

    const uint8_t* srcPixels = static_cast<const uint8_t*>(rgbaData);
    const std::vector<uint8_t> paddedPixels = makePaddedImage(
        srcPixels, storagePtr.rect.width, storagePtr.rect.height, kTexturePadding);
    const std::vector<MipLevelData> mipChain =
        buildMipChain(paddedPixels.data(), outerWidth, outerHeight);

    for (uint8_t mip = 0; mip < mipChain.size(); ++mip)
    {
        const MipLevelData& level = mipChain[mip];
        const uint16_t dstX = outerX >> mip;
        const uint16_t dstY = outerY >> mip;
        const bgfx::Memory* mem =
            bgfx::copy(level.pixels.data(), uint32_t(level.pixels.size()));

        bgfx::updateTexture2D(texIdent.texHandle,
                              texIdent.layerIdx,
                              mip,
                              dstX,
                              dstY,
                              level.width,
                              level.height,
                              mem);
    }

    glm::vec4 relBounds = {(float)storagePtr.rect.x / (float)texWidth,
                           (float)storagePtr.rect.y / (float)texHeight,
                           (float)storagePtr.rect.width / (float)texWidth,
                           (float)storagePtr.rect.height / (float)texHeight};
    Texture texture(name, path, texIdent, storagePtr, atlasHandle, relBounds);
    TextureHandle handle = textureLib.addItem(name, texture);
    LG_I("Texture has been added to GPU storage");
    LG_I("GPU Texture handle: {}, Layer: {}",
         texture.getTexIdent().texHandle.idx,
         texture.getTexIdent().layerIdx);
    LG_I("Texture name: {}, Lib idx: {}, Lib size: {}",
         name,
         handle.getIdx(),
         textureLib.size());
    LG_I("Atlas ID: {}, Pos: ({},{}) - {}x{}",
         atlasHandle.value(),
         texture.getStoragePtr().rect.x,
         texture.getStoragePtr().rect.y,
         texture.getStoragePtr().rect.width,
         texture.getStoragePtr().rect.height);
    LG_I("Wrapped texture handle: {} (idx: {}, gen: {})",
         handle.value(),
         handle.getIdx(),
         handle.getGeneration());
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
            TextureAtlasHandleUuid handle = textureAtlasLib.addWithRandomKey(
                TextureAtlas(texWidth,
                             texHeight,
                             bucketSize,
                             texIdent,
                             excessHeightThreshold));
            LG_D("Created new texture atlas with id {} for type {}",
                 handle.handle.getIdx(),
                 type);
            if (handle.handle.isValid())
            {
                atlasRegistry[type].push_back(handle.handle.getIdx());
                return handle.handle;
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

std::vector<std::string> TextureLoader::getTextureNames() const
{
    std::vector<std::string> names;
    const auto items = textureLib.getItems();
    names.reserve(items.size());
    for (const Texture* texture : items)
    {
        if (texture != nullptr)
        {
            names.push_back(texture->getName());
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

TextureHandle TextureLoader::getTextureHandle(const std::string& name)
{
    return textureLib.getHandle(name);
}

}  // namespace gfx

// Explicitly instantiate ItemLib<T> to ensure Handle is fully defined
// Must be outside the gfx namespace
template class con::ItemLib<gfx::TextureAtlas>;
template class con::ItemLib<gfx::Texture>;