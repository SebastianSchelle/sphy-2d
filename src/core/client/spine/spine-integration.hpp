#ifndef SPINE_INTEGRATION_HPP
#define SPINE_INTEGRATION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <spine/spine.h>
#include <render-engine.hpp>

namespace gfx
{

class SpineExtension final : public spine::DefaultSpineExtension
{
  public:
    SpineExtension() = default;
    ~SpineExtension() override = default;
};

enum class SpineBlendMode
{
    Normal,
    Additive,
    Multiply,
    Screen
};

class SpineTextureLoader final : public spine::TextureLoader
{
  public:
    SpineTextureLoader() = default;
    ~SpineTextureLoader() override = default;

    void load(spine::AtlasPage& page, const spine::String& path) override;
    void unload(void* texture) override;
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

    SpineBlendMode blendMode = SpineBlendMode::Normal;
};

class SpineIntegration
{
  public:
    SpineIntegration() = default;
    ~SpineIntegration();

    SpineIntegration(const SpineIntegration&) = delete;
    SpineIntegration& operator=(const SpineIntegration&) = delete;

    bool loadAtlas(const std::string& atlasPath);

    bool loadSkeletonJson(const std::string& skeletonPath, float scale = 1.0f);

    bool loadSkeletonBinary(const std::string& skeletonPath,
                            float scale = 1.0f);

    void unload();

    bool createSkeleton();
    void destroySkeleton();

    void update(float deltaTime);

    bool setAnimation(int track, const std::string& animation, bool loop);

    void render();

    spine::Skeleton* skeleton()
    {
        return skeleton_;
    }

    const spine::Skeleton* skeleton() const
    {
        return skeleton_;
    }

  private:
    std::unique_ptr<SpineTextureLoader> textureLoader_;
    std::unique_ptr<spine::Atlas> atlas_;

    spine::SkeletonData* skeletonData_ = nullptr;
    spine::AnimationStateData* animationStateData_ = nullptr;

    spine::Skeleton* skeleton_ = nullptr;
    spine::AnimationState* animationState_ = nullptr;

    spine::SkeletonRenderer renderer_;

  private:
    static TextureHandle
    engineLoadTexture(const char* path, int& width, int& height);

    static void engineUnloadTexture(TextureHandle texture);

    static void engineSubmit(const SpineDrawCommand& command);

    static SpineBlendMode convertBlendMode(spine::BlendMode blendMode);

    static void
    transformUVs(float* uvs, int numVertices, TextureHandle texture);
};

}  // namespace gfx

#endif