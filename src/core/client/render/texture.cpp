#include "texture.hpp"
#include <algorithm>
#include <bgfx/bgfx.h>
#include <cstring>
#include <glm/glm.hpp>
#include <bimg/bimg.h>
#include <bx/bx.h>
#include <bx/readerwriter.h>
#include <vector>

namespace gfx
{

constexpr uint16_t kTexturePadding = 8;
/// Extra clamp-replicated band beyond `kTexturePadding` (still edge-colored, not empty
/// atlas). Gives bilinear more same-colored texels before silhouette / neighbor tiles.
/// Slightly wider so coarse mips still carry replicated edge after 2× downsampling.
constexpr uint16_t kAtlasEdgeReplicaGutterPx = 6;

/// Per-slot right/bottom gutter (mip 0), filled by edge extrusion each mip so
/// trilinear sampling never averages empty / neighbor-tile texels. Uploads are
/// clamped to atlas mip bounds so nothing writes past the texture edge.
/// Larger base → `ceil(base/2^m)` stays ≥2 a bit longer at high mips.
constexpr int kAtlasMipExtrusionPx = 16;

constexpr int atlasEdgeReplicaRing()
{
    return int(kTexturePadding) + int(kAtlasEdgeReplicaGutterPx);
}

constexpr int fontAtlasReplicaRing()
{
    return int(kTexturePadding);
}

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

/// Straight RGBA8 → premultiplied RGBA8 (GPU-friendly mips and filtering).
static void premultiplyRgba8InPlace(std::vector<uint8_t>& rgba)
{
    for (size_t i = 0; i < rgba.size(); i += 4)
    {
        const uint32_t a = rgba[i + 3];
        rgba[i + 0] = static_cast<uint8_t>((uint32_t(rgba[i + 0]) * a + 127u) / 255u);
        rgba[i + 1] = static_cast<uint8_t>((uint32_t(rgba[i + 1]) * a + 127u) / 255u);
        rgba[i + 2] = static_cast<uint8_t>((uint32_t(rgba[i + 2]) * a + 127u) / 255u);
    }
}

/// Build mip chain in straight RGBA (alpha-weighted 2×2), then caller premultiplies.
static std::vector<MipLevelData> buildMipChainStraightAlphaAware(const uint8_t* rgbaData,
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
                const uint32_t asum = a00 + a10 + a01 + a11;

                if (asum == 0)
                {
                    dstPixels[di + 0] = 0;
                    dstPixels[di + 1] = 0;
                    dstPixels[di + 2] = 0;
                    dstPixels[di + 3] = 0;
                    continue;
                }

                const uint32_t r00 = src.pixels[i00 + 0];
                const uint32_t g00 = src.pixels[i00 + 1];
                const uint32_t b00 = src.pixels[i00 + 2];
                const uint32_t r10 = src.pixels[i10 + 0];
                const uint32_t g10 = src.pixels[i10 + 1];
                const uint32_t b10 = src.pixels[i10 + 2];
                const uint32_t r01 = src.pixels[i01 + 0];
                const uint32_t g01 = src.pixels[i01 + 1];
                const uint32_t b01 = src.pixels[i01 + 2];
                const uint32_t r11 = src.pixels[i11 + 0];
                const uint32_t g11 = src.pixels[i11 + 1];
                const uint32_t b11 = src.pixels[i11 + 2];

                const uint32_t numR = r00 * a00 + r10 * a10 + r01 * a01 + r11 * a11;
                const uint32_t numG = g00 * a00 + g10 * a10 + g01 * a01 + g11 * a11;
                const uint32_t numB = b00 * a00 + b10 * a10 + b01 * a01 + b11 * a11;

                dstPixels[di + 0] = static_cast<uint8_t>((numR + asum / 2) / asum);
                dstPixels[di + 1] = static_cast<uint8_t>((numG + asum / 2) / asum);
                dstPixels[di + 2] = static_cast<uint8_t>((numB + asum / 2) / asum);
                dstPixels[di + 3] = static_cast<uint8_t>((asum + 2) / 4);
            }
        }

        chain.push_back({std::move(dstPixels), dstWidth, dstHeight});
    }

    return chain;
}

static uint32_t textureMipDimension(uint16_t fullSize, uint8_t mip)
{
    uint32_t d = uint32_t(fullSize) >> mip;
    return d == 0 ? 1u : d;
}

static uint16_t mipExtrusionTexels(uint8_t mip, int baseTexels)
{
    if (baseTexels <= 0 || mip > 30)
        return 0;
    const uint32_t sh = 1u << mip;
    const uint32_t n = (uint32_t(baseTexels) + sh - 1u) >> mip;
    return n > 65535u ? 0 : static_cast<uint16_t>(n);
}

/// Right-edge extrusion strip; returns column count actually uploaded (0 if none).
static uint16_t uploadMipRightExtrusion(bgfx::TextureHandle texHandle,
                                        uint8_t layer,
                                        uint8_t mip,
                                        uint16_t dstGapX,
                                        uint16_t dstY,
                                        const MipLevelData& level,
                                        uint16_t edgeColX,
                                        uint16_t stripHeight,
                                        int baseExtrusionTexels,
                                        uint32_t texMipW)
{
    const uint16_t lw = level.width;
    const uint16_t lh = level.height;
    if (lw < 1 || lh < 1 || texMipW == 0 || stripHeight < 1)
        return 0;
    if (uint32_t(dstGapX) >= texMipW)
        return 0;

    const uint16_t srcGapCols = mipExtrusionTexels(mip, baseExtrusionTexels);
    if (srcGapCols == 0)
        return 0;

    const uint32_t room = texMipW - uint32_t(dstGapX);
    const uint16_t gapCols =
        static_cast<uint16_t>(std::min<uint32_t>(srcGapCols, room));
    if (gapCols == 0)
        return 0;

    const uint16_t col = std::min<uint16_t>(edgeColX, uint16_t(lw - 1u));
    const uint16_t h = std::min<uint16_t>(stripHeight, lh);

    const size_t bytes = static_cast<size_t>(gapCols) * static_cast<size_t>(h) * 4u;
    std::vector<uint8_t> gapPixels(bytes);
    for (uint16_t y = 0; y < h; ++y)
    {
        const uint32_t srcIdx = (uint32_t(y) * uint32_t(lw) + uint32_t(col)) * 4u;
        for (uint32_t x = 0; x < uint32_t(gapCols); ++x)
        {
            const uint32_t dstIdx =
                (uint32_t(y) * uint32_t(gapCols) + x) * 4u;
            gapPixels[dstIdx + 0] = level.pixels[srcIdx + 0];
            gapPixels[dstIdx + 1] = level.pixels[srcIdx + 1];
            gapPixels[dstIdx + 2] = level.pixels[srcIdx + 2];
            gapPixels[dstIdx + 3] = level.pixels[srcIdx + 3];
        }
    }

    const bgfx::Memory* mem = bgfx::copy(gapPixels.data(), uint32_t(bytes));
    bgfx::updateTexture2D(texHandle,
                          layer,
                          mip,
                          dstGapX,
                          dstY,
                          gapCols,
                          h,
                          mem,
                          static_cast<uint16_t>(gapCols * 4u));
    return gapCols;
}

static void uploadMipBottomExtrusion(bgfx::TextureHandle texHandle,
                                     uint8_t layer,
                                     uint8_t mip,
                                     uint16_t dstX,
                                     uint16_t dstGapY,
                                     const MipLevelData& level,
                                     uint16_t rightGapCols,
                                     uint16_t edgeRowY,
                                     int baseExtrusionTexels,
                                     uint32_t texMipW,
                                     uint32_t texMipH)
{
    const uint16_t lw = level.width;
    const uint16_t lh = level.height;
    if (lw < 1 || lh < 1 || texMipW == 0 || texMipH == 0)
        return;
    if (uint32_t(dstGapY) >= texMipH)
        return;

    const uint16_t srcGapRows = mipExtrusionTexels(mip, baseExtrusionTexels);
    if (srcGapRows == 0)
        return;

    const uint32_t roomRows = texMipH - uint32_t(dstGapY);
    const uint16_t gapRows =
        static_cast<uint16_t>(std::min<uint32_t>(srcGapRows, roomRows));
    if (gapRows == 0)
        return;

    const uint32_t rowTexels =
        std::min<uint32_t>(uint32_t(lw) + uint32_t(rightGapCols),
                           texMipW - uint32_t(dstX));
    if (rowTexels == 0)
        return;

    const uint16_t rowY = std::min<uint16_t>(edgeRowY, uint16_t(lh - 1u));

    const size_t rowBytes = static_cast<size_t>(rowTexels) * 4u;
    const size_t bytes = rowBytes * static_cast<size_t>(gapRows);
    std::vector<uint8_t> gapPixels(bytes);
    for (uint16_t r = 0; r < gapRows; ++r)
    {
        uint8_t* dstRow = gapPixels.data() + static_cast<size_t>(r) * rowBytes;
        for (uint32_t x = 0; x < rowTexels; ++x)
        {
            const uint16_t sx = static_cast<uint16_t>(
                std::min<uint32_t>(x, uint32_t(lw) - 1u));
            const uint32_t srcIdx =
                (uint32_t(rowY) * uint32_t(lw) + uint32_t(sx)) * 4u;
            const uint32_t dstIdx = x * 4u;
            dstRow[dstIdx + 0] = level.pixels[srcIdx + 0];
            dstRow[dstIdx + 1] = level.pixels[srcIdx + 1];
            dstRow[dstIdx + 2] = level.pixels[srcIdx + 2];
            dstRow[dstIdx + 3] = level.pixels[srcIdx + 3];
        }
    }

    const bgfx::Memory* mem = bgfx::copy(gapPixels.data(), uint32_t(bytes));
    bgfx::updateTexture2D(texHandle,
                          layer,
                          mip,
                          dstX,
                          dstGapY,
                          static_cast<uint16_t>(rowTexels),
                          gapRows,
                          mem,
                          static_cast<uint16_t>(rowTexels * 4u));
}

/// UV origin + span for `atlasUv = origin + quad01 * span` (vs_texrect / fs_geom).
/// Half-texel inset keeps linear sampling off atlas neighbors outside the slot (black fringe).
static glm::vec4 atlasUvOriginSpan(int innerX,
                                   int innerY,
                                   int innerW,
                                   int innerH,
                                   int atlasW,
                                   int atlasH)
{
    const float aw = float(std::max(1, atlasW));
    const float ah = float(std::max(1, atlasH));
    const float minU = (float(innerX) + 0.5f) / aw;
    const float minV = (float(innerY) + 0.5f) / ah;
    const float spanU =
        innerW > 1 ? (float(innerW) - 1.f) / aw : 1.f / aw;
    const float spanV =
        innerH > 1 ? (float(innerH) - 1.f) / ah : 1.f / ah;
    return glm::vec4(minU, minV, spanU, spanV);
}
}  // namespace

Texture::Texture(const std::string& name,
                 const std::string& path,
                 const TextureIdentifier& texIdent,
                 const StoragePtr& storagePtr,
                 TextureAtlasHandle atlasHandle,
                 glm::vec4 relBounds,
                 bool pointSample)
    : name(name),
      path(path),
      texIdent(texIdent),
      storagePtr(storagePtr),
      atlasHandle(atlasHandle),
      relBounds(relBounds),
      pointSample(pointSample)
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

TextureArray::TextureArray(uint16_t width,
                           uint16_t height,
                           uint8_t layerCnt,
                           bool generateMipmaps)
{
    this->width = width;
    this->height = height;
    this->layerCnt = 0;
    this->maxLayerCnt = layerCnt;
    this->handle = BGFX_INVALID_HANDLE;
    this->generateMipmaps = generateMipmaps;
}

TextureArray::~TextureArray() {}

bool TextureArray::init()
{
    // Do not overwrite existing texture array
    if (!bgfx::isValid(handle))
    {
        handle = bgfx::createTexture2D((uint16_t)width,
                                       (uint16_t)height,
                                       generateMipmaps,
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

void TextureArray::getGpuInfo(GpuTextureArrayInfo& out) const
{
    out.handle = handle;
    out.width = width;
    out.height = height;
    out.layersUsed = layerCnt;
    out.layersCapacity = maxLayerCnt;
    out.hasMipmaps = generateMipmaps;
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

bool TextureLoader::isFontAtlasType(const std::string& type)
{
    return type == kAtlasTypeRmlUiFont;
}

TextureHandle TextureLoader::generateTexture(const std::string& name,
                                             const std::string& type,
                                             const void* data,
                                             int width,
                                             int height,
                                             const std::string& path)
{
    StoragePtr storagePtr;
    const bool fontGlyph = isFontAtlasType(type);
    const int ring = fontGlyph ? fontAtlasReplicaRing() : atlasEdgeReplicaRing();
    storagePtr.rect.width =
        uint16_t(width + ring * 2 + (fontGlyph ? 0 : kAtlasMipExtrusionPx));
    storagePtr.rect.height =
        uint16_t(height + ring * 2 + (fontGlyph ? 0 : kAtlasMipExtrusionPx));
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
                                       data,
                                       fontGlyph);
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
                                       rgbaData,
                                       isFontAtlasType(type));
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
                                         const void* rgbaData,
                                         bool fontGlyph)
{
    const uint16_t outerX = storagePtr.rect.x;
    const uint16_t outerY = storagePtr.rect.y;

    const int ring =
        fontGlyph ? fontAtlasReplicaRing() : atlasEdgeReplicaRing();
    const int gutter = fontGlyph ? 0 : kAtlasMipExtrusionPx;
    storagePtr.rect.x += ring;
    storagePtr.rect.y += ring;
    storagePtr.rect.width -= ring * 2 + gutter;
    storagePtr.rect.height -= ring * 2 + gutter;

    const uint8_t* srcPixels = static_cast<const uint8_t*>(rgbaData);
    std::vector<uint8_t> paddedPixels = makePaddedImage(
        srcPixels, storagePtr.rect.width, storagePtr.rect.height, uint16_t(ring));
    const uint16_t tileW = static_cast<uint16_t>(storagePtr.rect.width + ring * 2);
    const uint16_t tileH = static_cast<uint16_t>(storagePtr.rect.height + ring * 2);
    if (size_t(tileW) * size_t(tileH) * 4u != paddedPixels.size())
    {
        LG_E("makeTexture: padded size mismatch ({}x{} vs {} bytes)",
             int(tileW),
             int(tileH),
             paddedPixels.size());
        return TextureHandle::Invalid();
    }
    std::vector<MipLevelData> mipChain;
    if (fontGlyph)
    {
        mipChain.push_back({std::move(paddedPixels), tileW, tileH});
    }
    else
    {
        mipChain =
            buildMipChainStraightAlphaAware(paddedPixels.data(), tileW, tileH);
    }
    for (MipLevelData& level : mipChain)
    {
        premultiplyRgba8InPlace(level.pixels);
    }

    const uint8_t mipCount = fontGlyph ? 1u : static_cast<uint8_t>(mipChain.size());
    for (uint8_t mip = 0; mip < mipCount; ++mip)
    {
        const MipLevelData& level = mipChain[mip];
        const uint16_t dstX = fontGlyph ? outerX : uint16_t(outerX >> mip);
        const uint16_t dstY = fontGlyph ? outerY : uint16_t(outerY >> mip);
        const uint32_t mipW = textureMipDimension(texWidth, mip);
        const uint32_t mipH = textureMipDimension(texHeight, mip);

        if (uint32_t(dstX) >= mipW || uint32_t(dstY) >= mipH)
        {
            LG_W("makeTexture: mip {} dst ({},{}) outside {}x{} — skip",
                 int(mip),
                 int(dstX),
                 int(dstY),
                 int(mipW),
                 int(mipH));
            continue;
        }

        const uint32_t maxW = mipW - uint32_t(dstX);
        const uint32_t maxH = mipH - uint32_t(dstY);
        const uint32_t upW = std::min<uint32_t>(level.width, maxW);
        const uint32_t upH = std::min<uint32_t>(level.height, maxH);
        if (upW == 0 || upH == 0)
            continue;

        const bgfx::Memory* mem = nullptr;
        if (upW == uint32_t(level.width) && upH == uint32_t(level.height))
        {
            mem = bgfx::copy(level.pixels.data(), uint32_t(level.pixels.size()));
        }
        else
        {
            std::vector<uint8_t> sub(upW * upH * 4u);
            for (uint32_t y = 0; y < upH; ++y)
            {
                std::memcpy(sub.data() + y * upW * 4u,
                            level.pixels.data()
                                + (y * uint32_t(level.width)) * 4u,
                            upW * 4u);
            }
            mem = bgfx::copy(sub.data(), uint32_t(sub.size()));
        }

        bgfx::updateTexture2D(texIdent.texHandle,
                              texIdent.layerIdx,
                              mip,
                              dstX,
                              dstY,
                              static_cast<uint16_t>(upW),
                              static_cast<uint16_t>(upH),
                              mem);

        if (!fontGlyph)
        {
            const uint16_t gapX =
                static_cast<uint16_t>(dstX + static_cast<uint16_t>(upW));
            const uint16_t edgeCol =
                static_cast<uint16_t>(upW >= 1u ? upW - 1u : 0u);
            const uint16_t rightWritten = uploadMipRightExtrusion(
                texIdent.texHandle,
                texIdent.layerIdx,
                mip,
                gapX,
                dstY,
                level,
                edgeCol,
                static_cast<uint16_t>(upH),
                kAtlasMipExtrusionPx,
                mipW);

            const uint16_t gapY =
                static_cast<uint16_t>(dstY + static_cast<uint16_t>(upH));
            const uint16_t edgeRow =
                static_cast<uint16_t>(upH >= 1u ? upH - 1u : 0u);
            uploadMipBottomExtrusion(texIdent.texHandle,
                                     texIdent.layerIdx,
                                     mip,
                                     dstX,
                                     gapY,
                                     level,
                                     rightWritten,
                                     edgeRow,
                                     kAtlasMipExtrusionPx,
                                     mipW,
                                     mipH);
        }
    }

    glm::vec4 relBounds = atlasUvOriginSpan(storagePtr.rect.x,
                                            storagePtr.rect.y,
                                            storagePtr.rect.width,
                                            storagePtr.rect.height,
                                            int(texWidth),
                                            int(texHeight));
    Texture texture(
        name, path, texIdent, storagePtr, atlasHandle, relBounds, fontGlyph);
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
    const bool mips = !isFontAtlasType(type);
    // Check if a free layer is available in a matching texture array
    for (auto& textureArray : textureArrays)
    {
        if (textureArray.hasMipmaps() != mips)
        {
            continue;
        }
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
    TextureArray textureArray(texWidth, texHeight, texLayerCnt, mips);
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

size_t TextureLoader::getGpuTextureArrayCount() const
{
    return textureArrays.size();
}

bool TextureLoader::getGpuTextureArrayInfo(size_t index,
                                           GpuTextureArrayInfo& out) const
{
    if (index >= textureArrays.size())
    {
        return false;
    }
    textureArrays[index].getGpuInfo(out);
    return bgfx::isValid(out.handle);
}

int TextureLoader::gpuArrayIndexOfHandle(bgfx::TextureHandle h) const
{
    if (!bgfx::isValid(h))
    {
        return -1;
    }
    for (size_t i = 0; i < textureArrays.size(); ++i)
    {
        GpuTextureArrayInfo g{};
        textureArrays[i].getGpuInfo(g);
        if (bgfx::isValid(g.handle) && g.handle.idx == h.idx)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string TextureLoader::getAtlasRegistrySummary() const
{
    if (atlasRegistry.empty())
    {
        return "(no atlas types registered yet)";
    }
    std::vector<std::string> keys;
    keys.reserve(atlasRegistry.size());
    for (const auto& p : atlasRegistry)
    {
        keys.push_back(p.first);
    }
    std::sort(keys.begin(), keys.end());
    std::string s;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        if (i)
        {
            s += ", ";
        }
        s += keys[i];
    }
    return s;
}

void TextureLoader::fillAtlasDebugGpuArrayOptions(
    std::vector<AtlasDebugSelectOption>& out) const
{
    out.clear();
    for (size_t i = 0; i < textureArrays.size(); ++i)
    {
        GpuTextureArrayInfo g{};
        textureArrays[i].getGpuInfo(g);
        AtlasDebugSelectOption o;
        o.value = static_cast<int>(i);
        o.label = "GPU " + std::to_string(o.value) + " — "
                  + std::to_string(int(g.width)) + "×"
                  + std::to_string(int(g.height)) + " — "
                  + std::to_string(int(g.layersUsed)) + "/"
                  + std::to_string(int(g.layersCapacity)) + " layers";
        out.push_back(std::move(o));
    }
}

void TextureLoader::fillAtlasDebugLayerOptions(
    int gpuArrayIndex,
    std::vector<AtlasDebugSelectOption>& out) const
{
    out.clear();
    GpuTextureArrayInfo g{};
    if (gpuArrayIndex < 0
        || !getGpuTextureArrayInfo(static_cast<size_t>(gpuArrayIndex), g))
    {
        return;
    }
    const int n = std::max(0, int(g.layersUsed) - 1);
    for (int li = 0; li <= n; ++li)
    {
        AtlasDebugSelectOption o;
        o.value = li;
        o.label = "Layer " + std::to_string(li);
        out.push_back(std::move(o));
    }
}

void TextureLoader::fillAtlasDebugMipOptions(
    int gpuArrayIndex,
    std::vector<AtlasDebugSelectOption>& out) const
{
    out.clear();
    GpuTextureArrayInfo g{};
    if (gpuArrayIndex < 0
        || !getGpuTextureArrayInfo(static_cast<size_t>(gpuArrayIndex), g))
    {
        return;
    }
    if (!g.hasMipmaps)
    {
        AtlasDebugSelectOption o;
        o.value = 0;
        o.label = "Mip 0 (no mip chain)";
        out.push_back(std::move(o));
        return;
    }
    uint32_t mw = g.width;
    uint32_t mh = g.height;
    for (int mip = 0;; ++mip)
    {
        const uint32_t dw = std::max(1u, mw >> mip);
        const uint32_t dh = std::max(1u, mh >> mip);
        AtlasDebugSelectOption o;
        o.value = mip;
        o.label = "Mip " + std::to_string(mip) + " (" + std::to_string(dw)
                  + "×" + std::to_string(dh) + ")";
        out.push_back(o);
        if (dw <= 1u && dh <= 1u)
        {
            break;
        }
    }
}

void TextureLoader::fillAtlasDebugKindPickRows(
    std::vector<AtlasDebugKindPickRow>& out)
{
    out.clear();
    AtlasDebugKindPickRow manual;
    manual.label = "— Manual (GPU / layer below) —";
    manual.gpuArrayIndex = -1;
    manual.layerIndex = 0;
    out.push_back(std::move(manual));

    std::vector<std::string> keys;
    keys.reserve(atlasRegistry.size());
    for (const auto& p : atlasRegistry)
    {
        keys.push_back(p.first);
    }
    std::sort(keys.begin(), keys.end());
    for (const std::string& type : keys)
    {
        auto it = atlasRegistry.find(type);
        if (it == atlasRegistry.end() || it->second.empty())
        {
            continue;
        }
        TextureAtlas* atlas = textureAtlasLib.getItem(it->second[0]);
        if (!atlas)
        {
            continue;
        }
        const TextureIdentifier tid = atlas->getTexIdent();
        const int ai = gpuArrayIndexOfHandle(tid.texHandle);
        if (ai < 0)
        {
            continue;
        }
        AtlasDebugKindPickRow row;
        row.gpuArrayIndex = ai;
        row.layerIndex = int(tid.layerIdx);
        row.label = type + " → GPU " + std::to_string(ai) + ", layer "
                    + std::to_string(row.layerIndex);
        out.push_back(std::move(row));
    }
}

}  // namespace gfx

// Explicitly instantiate ItemLib<T> to ensure Handle is fully defined
// Must be outside the gfx namespace
template class con::ItemLib<gfx::TextureAtlas>;
template class con::ItemLib<gfx::Texture>;