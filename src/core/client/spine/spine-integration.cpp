#include "spine-integration.hpp"
#include "spine/AnimationStateData.h"
#include "spine/Atlas.h"
#include "texture.hpp"
#include <cstdio>
#include <render-engine.hpp>

namespace gfx
{


SkeletonInstance AnimationData::createSkeleton()
{
    if (!loaded())
    {
        LG_E("Invalid animation data. Could not create skeleton");
        return SkeletonInstance();
    }
    auto skeleton = new spine::Skeleton(*skeletonData);
    auto animationState = new spine::AnimationState(*animationData);
    if (!skeleton || !animationState)
    {
        LG_E("Failed to create skeleton");
        return SkeletonInstance();
    }
    return SkeletonInstance(skeleton, animationState);
}

bool SkeletonInstance::update(float delta)
{
    if (!loaded())
    {
        LG_E("Updating skeleton failed. Invalid pointers.");
        return false;
    }
    animationState->update(delta);
    animationState->apply(*skeleton);
    skeleton->update(delta);
    skeleton->updateWorldTransform(spine::Physics_Update);
    return true;
}

bool SkeletonInstance::setAnimation(int track,
                                    const string& animation,
                                    bool loop)
{
    if (!loaded())
    {
        LG_E("Setting animation failed. Invalid pointers.");
        return false;
    }
    if (!skeleton->getData().findAnimation(animation.c_str()))
    {
        LG_E("Animation {} does not exist", animation);
        return false;
    }
    animationState->setAnimation(track, animation.c_str(), loop);
    return true;
}

bool SkeletonInstance::render(SpineIntegration& spineIntegration)
{
    if (!loaded())
    {
        LG_E("Rendering skeleton failed. Invalid pointers.");
        return false;
    }
    spine::RenderCommand* cmd = spineIntegration.skelRenderer.render(*skeleton);
    while (cmd)
    {
        SpineDrawCommand draw{};
        draw.positions = cmd->positions;
        draw.uvs = cmd->uvs;
        draw.colors = cmd->colors;
        draw.indices = cmd->indices;
        draw.numVertices = cmd->numVertices;
        draw.numIndices = cmd->numIndices;
        draw.texture = TextureHandle(
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cmd->texture)));
        draw.blendMode = spineIntegration.convertBlendMode(cmd->blendMode);
        spineIntegration.engineSubmit(draw);
        cmd = cmd->next;
    }
    return true;
}

// ============================================================================
// SpineTextureLoader
// ============================================================================

void SpineTextureLoader::load(spine::AtlasPage& page, const spine::String& path)
{
    std::string uuid = sec::uuid();
    vec2 dim;
    auto texHandle = renderer->loadTexture(uuid, "spine", path.buffer(), dim);
    if (!texHandle.isValid())
    {
        LG_E("Failed to load texture {} for spine atlas", path.buffer());
        return;
    }
    page.texture =
        reinterpret_cast<void*>(static_cast<uintptr_t>(texHandle.value()));
    page.width = dim.x;
    page.height = dim.y;

    LG_I("Loaded spine texture {} with dimensions ({}, {}); Handle: {}",
         path.buffer(),
         page.width,
         page.height,
         texHandle.toString());
}

void SpineTextureLoader::unload(void* texture)
{
    if (!texture)
        return;

    const auto id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(texture));
    // todo: implement unloading
}

// ============================================================================
// SpineIntegration
// ============================================================================

spine::Atlas* SpineIntegration::loadAtlas(const std::string& atlasPath)
{
    auto textureLoader = new SpineTextureLoader(renderer);
    spine::Atlas* atlas = new spine::Atlas(atlasPath.c_str(), textureLoader);
    if (!atlas)
    {
        std::fprintf(
            stderr, "Failed to load Spine atlas: %s\n", atlasPath.c_str());
        return nullptr;
    }
    auto& pages = atlas->getPages();

    LG_I("Atlas {} has {} pages", atlasPath, pages.size());
    return atlas;
}

AnimationData
SpineIntegration::loadSkeletonJson(spine::Atlas* atlas,
                                   const std::string& skeletonPath,
                                   float scale)
{
    if (!atlas)
    {
        std::fprintf(stderr,
                     "Spine atlas must be loaded before skeleton data.\n");

        return AnimationData();
    }
    spine::SkeletonJson json(*atlas);
    json.setScale(scale);
    auto skeletonData = json.readSkeletonDataFile(skeletonPath.c_str());
    if (!skeletonData)
    {
        LG_E("Failed to load Spine skeleton: {}; Error: {}",
             skeletonPath.c_str(),
             json.getError().buffer());
        return AnimationData();
    }
    auto animationStateData = new spine::AnimationStateData(*skeletonData);
    return AnimationData(skeletonData, animationStateData);
}

AnimationData
SpineIntegration::loadSkeletonBinary(spine::Atlas* atlas,
                                     const std::string& skeletonPath,
                                     float scale)
{
    if (!atlas)
    {
        std::fprintf(stderr,
                     "Spine atlas must be loaded before skeleton data.\n");
        return AnimationData();
    }
    spine::SkeletonBinary binary(*atlas);
    binary.setScale(scale);
    auto skeletonData = binary.readSkeletonDataFile(skeletonPath.c_str());
    if (!skeletonData)
    {
        LG_E("Failed to load Spine skeleton: {}; Error: {}",
             skeletonPath.c_str(),
             binary.getError().buffer());
        return AnimationData();
    }
    auto animationStateData = new spine::AnimationStateData(*skeletonData);
    return AnimationData(skeletonData, animationStateData);
}


AnimationData SpineIntegration::loadAnimationBinary(const string& atlasPath,
                                                    const string& skeletonPath,
                                                    float scale)
{
    if (auto atlas = loadAtlas(atlasPath))
    {
        return loadSkeletonBinary(atlas, skeletonPath, scale);
    }
    return AnimationData();
}

void SpineIntegration::unload(const AnimationData& animationData)
{
    delete animationData.animationData;
    delete animationData.skeletonData;
}

void SpineIntegration::destroySkeleton(const SkeletonInstance& skeleton)
{
    delete skeleton.animationState;
    delete skeleton.skeleton;
}

void SpineIntegration::engineSubmit(const SpineDrawCommand& cmd)
{
    renderer->queueSpine(cmd, 0.0f, 0);
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
