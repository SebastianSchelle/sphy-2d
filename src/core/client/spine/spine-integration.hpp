#ifndef SPINE_INTEGRATION_HPP
#define SPINE_INTEGRATION_HPP

#include "spine/AnimationState.h"
#include "spine/AnimationStateData.h"
#include "spine/Skeleton.h"
#include "spine/SkeletonData.h"
#include "spine/SkeletonRenderer.h"
#include <cstdint>
#include <item-lib.hpp>
#include <spine/spine.h>
#include <string>
#include <texture.hpp>

namespace gfx
{
class RenderEngine;
class SpineIntegration;
struct SkeletonInstance;

enum class SpineBlendMode
{
    Normal,
    Additive,
    Multiply,
    Screen
};

struct SpineDrawCommand
{
    const float* positions = nullptr;
    const float* uvs = nullptr;
    const uint32_t* colors = nullptr;
    const uint16_t* indices = nullptr;

    int numVertices = 0;
    int numIndices = 0;

    TextureHandle texture{};

    uint64_t blendMode = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                               BGFX_STATE_BLEND_INV_SRC_ALPHA);
};

struct AnimationData
{
    AnimationData() : skeletonData(nullptr), animationData(nullptr) {}
    AnimationData(spine::SkeletonData* skeletonData,
                  spine::AnimationStateData* animationData)
        : skeletonData(skeletonData), animationData(animationData)
    {
    }
    spine::SkeletonData* skeletonData;
    spine::AnimationStateData* animationData;

    bool loaded() const
    {
        return skeletonData && animationData;
    }

    SkeletonInstance createSkeleton();
};

struct SkeletonInstance
{
    SkeletonInstance() : skeleton(nullptr), animationState(nullptr) {}
    SkeletonInstance(spine::Skeleton* skeleton,
                     spine::AnimationState* animationState)
        : skeleton(skeleton), animationState(animationState)
    {
    }
    spine::Skeleton* skeleton;
    spine::AnimationState* animationState;

    bool loaded() const
    {
        return skeleton && animationState;
    }
    bool update(float delta);
    bool setAnimation(int track, const string& animation, bool loop);
    bool render(SpineIntegration& spineIntegration);
};

using AnimationDataHandle = typename con::ItemLib<AnimationData>::Handle;

class SpineExtension final : public spine::DefaultSpineExtension
{
  public:
    SpineExtension() = default;
    ~SpineExtension() override = default;
};

class SpineTextureLoader final : public spine::TextureLoader
{
  public:
    SpineTextureLoader(RenderEngine* renderer) : renderer(renderer) {}
    ~SpineTextureLoader() override = default;

    void load(spine::AtlasPage& page, const spine::String& path) override;
    void unload(void* texture) override;

  private:
    RenderEngine* renderer;
};

class SpineIntegration
{
  public:
    SpineIntegration(RenderEngine* renderer) : renderer(renderer) {};
    ~SpineIntegration() = default;

    SpineIntegration(const SpineIntegration&) = delete;
    SpineIntegration& operator=(const SpineIntegration&) = delete;

    AnimationData loadAnimationBinary(const string& atlasPath,
                                      const string& skeletonPath,
                                      float scale = 1.0f);

    spine::Atlas* loadAtlas(const string& atlasPath);

    AnimationData loadSkeletonJson(spine::Atlas* atlas,
                                   const string& skeletonPath,
                                   float scale = 1.0f);

    AnimationData loadSkeletonBinary(spine::Atlas* atlas,
                                     const std::string& skeletonPath,
                                     float scale = 1.0f);
    void unload(const AnimationData& animationData);

    SkeletonInstance createSkeleton(const AnimationData& animationData);
    void destroySkeleton(const SkeletonInstance& skeleton);
    static uint64_t convertBlendMode(spine::BlendMode blendMode);
    void engineSubmit(const SpineDrawCommand& command);

    spine::SkeletonRenderer skelRenderer;

  private:
    TextureHandle engineLoadTexture(const char* path, int& width, int& height);
    static void
    transformUVs(float* uvs, int numVertices, TextureHandle texture);

    RenderEngine* renderer;
};

}  // namespace gfx

#endif