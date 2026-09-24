#include "spine-integration.hpp"
#include "spine/AnimationStateData.h"
#include "texture.hpp"

#include <cstdio>

namespace gfx
{

// ============================================================================
// SpineTextureLoader
// ============================================================================

void SpineTextureLoader::load(spine::AtlasPage& page, const spine::String& path)
{
    int width = 0;
    int height = 0;

    /*
     * Load the ENTIRE Spine atlas page through the engine.
     *
     * Example:
     *
     *     character.atlas
     *     character.png
     *
     * character.png may contain many Spine regions.
     *
     * We do NOT split those regions here.
     *
     * The engine can pack the complete page into its own texture atlas.
     */
    // TextureHandle texture =
    //     SpineIntegration::engineLoadTexture(path.buffer(), width, height);

    /*
     * Spine expects AtlasPage::texture to contain the texture object
     * associated with this page.
     *
     * This example assumes TextureHandle is safely representable as
     * a pointer-sized integer. For a real engine, an engine-owned
     * SpinePageTexture object is preferable.
     */
    page.texture = reinterpret_cast<void*>(static_cast<uintptr_t>(0));

    page.width = width;
    page.height = height;
}

void SpineTextureLoader::unload(void* texture)
{
    if (!texture)
        return;

    const auto id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(texture));

    // SpineIntegration::engineUnloadTexture(TextureHandle{id});
}

// ============================================================================
// SpineIntegration
// ============================================================================

SpineIntegration::~SpineIntegration()
{
    unload();
}

bool SpineIntegration::loadAtlas(const std::string& atlasPath)
{
    unload();

    textureLoader_ = std::make_unique<SpineTextureLoader>();

    atlas_ = std::make_unique<spine::Atlas>(spine::String(atlasPath.c_str()),
                                            textureLoader_.get());

    if (!atlas_)
    {
        std::fprintf(
            stderr, "Failed to load Spine atlas: %s\n", atlasPath.c_str());

        return false;
    }

    return true;
}

bool SpineIntegration::loadSkeletonJson(const std::string& skeletonPath,
                                        float scale)
{
    if (!atlas_)
    {
        std::fprintf(stderr,
                     "Spine atlas must be loaded before skeleton data.\n");

        return false;
    }

    spine::SkeletonJson json(*atlas_);

    json.setScale(scale);

    skeletonData_ =
        json.readSkeletonDataFile(spine::String(skeletonPath.c_str()));

    if (!skeletonData_)
    {
        std::fprintf(stderr,
                     "Failed to load Spine skeleton: %s\nError: %s\n",
                     skeletonPath.c_str(),
                     json.getError().buffer());

        return false;
    }
    animationStateData_ = new spine::AnimationStateData(*skeletonData_);

    return true;
}

bool SpineIntegration::loadSkeletonBinary(const std::string& skeletonPath,
                                          float scale)
{
    if (!atlas_)
    {
        std::fprintf(stderr,
                     "Spine atlas must be loaded before skeleton data.\n");

        return false;
    }

    spine::SkeletonBinary binary(*atlas_);

    binary.setScale(scale);

    skeletonData_ =
        binary.readSkeletonDataFile(spine::String(skeletonPath.c_str()));

    if (!skeletonData_)
    {
        std::fprintf(stderr,
                     "Failed to load Spine skeleton: %s\nError: %s\n",
                     skeletonPath.c_str(),
                     binary.getError().buffer());

        return false;
    }

    animationStateData_ = new spine::AnimationStateData(*skeletonData_);

    return true;
}

void SpineIntegration::unload()
{
    destroySkeleton();

    delete animationStateData_;
    animationStateData_ = nullptr;

    delete skeletonData_;
    skeletonData_ = nullptr;

    atlas_.reset();
    textureLoader_.reset();
}

bool SpineIntegration::createSkeleton()
{
    if (!skeletonData_ || !animationStateData_)
        return false;

    destroySkeleton();

    skeleton_ = new spine::Skeleton(*skeletonData_);

    animationState_ = new spine::AnimationState(*animationStateData_);

    return true;
}

void SpineIntegration::destroySkeleton()
{
    delete animationState_;
    animationState_ = nullptr;

    delete skeleton_;
    skeleton_ = nullptr;
}

// ============================================================================
// Animation
// ============================================================================

void SpineIntegration::update(float deltaTime)
{
    if (!skeleton_ || !animationState_)
        return;

    animationState_->update(deltaTime);
    animationState_->apply(*skeleton_);

    skeleton_->update(deltaTime);
    skeleton_->updateWorldTransform(spine::Physics_Update);
}

bool SpineIntegration::setAnimation(int track,
                                    const std::string& animation,
                                    bool loop)
{
    if (!animationState_ || !skeletonData_)
        return false;

    if (!skeletonData_->findAnimation(spine::String(animation.c_str())))
    {
        return false;
    }

    animationState_->setAnimation(
        track, spine::String(animation.c_str()), loop);

    return true;
}

// ============================================================================
// Rendering
// ============================================================================

void SpineIntegration::render()
{
    if (!skeleton_)
        return;

    /*
     * Spine generates the render commands in the correct draw order.
     *
     * Do not sort these commands by texture/material in a way that
     * changes their ordering.
     */
    spine::RenderCommand* command = renderer_.render(*skeleton_);

    while (command)
    {
        SpineDrawCommand draw{};

        draw.positions = command->positions;
        draw.uvs = command->uvs;
        draw.colors = command->colors;
        draw.indices = command->indices;

        draw.numVertices = command->numVertices;
        draw.numIndices = command->numIndices;

        draw.texture =
            TextureHandle(static_cast<uint32_t>(
                              reinterpret_cast<uintptr_t>(command->texture)),
                          0);

        draw.blendMode = convertBlendMode(command->blendMode);

        /*
         * Spine UVs are relative to the Spine atlas page.
         *
         * If the entire page has been packed into your engine atlas,
         * transform those UVs here before submitting the command.
         *
         * transformUVs(
         *     const_cast<float*>(draw.uvs),
         *     draw.numVertices,
         *     draw.texture);
         */

        engineSubmit(draw);

        command = command->next;
    }
}

// ============================================================================
// Engine integration
// ============================================================================

TextureHandle
SpineIntegration::engineLoadTexture(const char* path, int& width, int& height)
{
    /*
     * TODO:
     *
     * Plug your texture manager in here.
     *
     * Example:
     *
     *     auto texture = textureManager.load(path);
     *
     *     width = textureManager.getWidth(texture);
     *     height = textureManager.getHeight(texture);
     *
     *     return texture;
     */

    std::printf("[Spine] load texture: %s\n", path);

    width = 0;
    height = 0;

    return {};
}

void SpineIntegration::engineUnloadTexture(TextureHandle texture)
{
    /*
     * TODO:
     *
     * Plug your texture manager release function in here.
     */

    std::printf("[Spine] unload texture: %u\n", texture.getIdx());
}

void SpineIntegration::engineSubmit(const SpineDrawCommand& command)
{
    /*
     * TODO:
     *
     * Convert this into your engine's RenderCommand.
     *
     * Preserve the order in which Spine submits commands.
     */

    (void)command;
}

SpineBlendMode SpineIntegration::convertBlendMode(spine::BlendMode blendMode)
{
    switch (blendMode)
    {
        case spine::BlendMode_Normal:
            return SpineBlendMode::Normal;

        case spine::BlendMode_Additive:
            return SpineBlendMode::Additive;

        case spine::BlendMode_Multiply:
            return SpineBlendMode::Multiply;

        case spine::BlendMode_Screen:
            return SpineBlendMode::Screen;

        default:
            return SpineBlendMode::Normal;
    }
}

void SpineIntegration::transformUVs(float* uvs,
                                    int numVertices,
                                    TextureHandle texture)
{
    /*
     * TODO:
     *
     * Query your texture manager for the atlas rectangle containing
     * the complete Spine page.
     *
     * For example:
     *
     *     pageX      = 1024
     *     pageY      = 256
     *     pageWidth  = 512
     *     pageHeight = 512
     *
     *     atlasWidth  = 4096
     *     atlasHeight = 4096
     *
     * Convert:
     *
     *     u = (pageX + u * pageWidth) / atlasWidth;
     *     v = (pageY + v * pageHeight) / atlasHeight;
     *
     * Spine has already generated the correct UVs for the regions
     * inside the page. We only translate them into engine-atlas space.
     */

    (void)uvs;
    (void)numVertices;
    (void)texture;
}

}  // namespace gfx


namespace spine
{

SpineExtension* getDefaultExtension()
{
    return new DefaultSpineExtension();
}

}  // namespace spine
