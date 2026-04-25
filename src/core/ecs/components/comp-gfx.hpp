#ifndef COMP_GFX_HPP
#define COMP_GFX_HPP

#include <entt/entt.hpp>
#include <mod-manager.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace ecs
{

struct MapIcon
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "map-icon";

    GenericHandle texHandle;
    vec2 size;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        MapIcon mapIcon;
        string texName;
        TRY_YAML_DICT(texName, node["texName"], "");
        mod::MappedTextureHandle mTexHandle =
            resourceMap.getTextureHandle(texName);
        if (!mTexHandle.isValid())
        {
            LG_E("Texture not found: {}", texName);
            return;
        }
        mapIcon.texHandle = *(GenericHandle*)&mTexHandle;
        TRY_YAML_DICT(mapIcon.size, node["size"], vec2(10.0f, 10.0f));
        registry.emplace<MapIcon>(entity, mapIcon);
    }
};

#define SER_MAP_ICON                                                           \
    SOBJ(o.texHandle);                                                         \
    SOBJ(o.size);
EXT_SER(MapIcon, SER_MAP_ICON)
EXT_DES(MapIcon, SER_MAP_ICON)

enum class TextureFlags : uint8_t
{
    FlipX = 0,
    FlipY = 1,
};

struct Texture
{
    GenericHandle texHandle;
    vec4 bounds;
    float rot;
    uint8_t flags;
    int8_t zIndex;
};

#define SER_TEXTURE                                                            \
    SOBJ(o.texHandle);                                                         \
    SOBJ(o.bounds);                                                            \
    S4b(o.rot);                                                                \
    S1b(o.flags);                                                              \
    S1b(o.zIndex);
EXT_SER(Texture, SER_TEXTURE)
EXT_DES(Texture, SER_TEXTURE)

struct Textures
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "textures";

    std::vector<Texture> textures;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        Textures textures;
        for (const auto& texNode : node["textures"])
        {
            Texture texture;
            string texName = "";
            TRY_YAML_DICT(texName, texNode["name"], "");
            mod::MappedTextureHandle mTexHandle =
                resourceMap.getTextureHandle(texName);
            if (!mTexHandle.isValid())
            {
                LG_W("Texture not found: {}", texName);
                texture.texHandle = *(GenericHandle*)&mTexHandle;
            }
            else
            {
                texture.texHandle = *(GenericHandle*)&mTexHandle;
            }
            TRY_YAML_DICT(texture.bounds.x, texNode["bounds"][0], 0.0f);
            TRY_YAML_DICT(texture.bounds.y, texNode["bounds"][1], 0.0f);
            TRY_YAML_DICT(texture.bounds.z, texNode["bounds"][2], 1.0f);
            TRY_YAML_DICT(texture.bounds.w, texNode["bounds"][3], 1.0f);
            TRY_YAML_DICT(texture.zIndex, texNode["zIndex"], 0);
            TRY_YAML_DICT(texture.flags, texNode["flags"], 0);
            TRY_YAML_DICT(texture.rot, texNode["rot"], 0.0f);
            texture.rot = smath::degToRad(texture.rot);
            textures.textures.push_back(texture);
        }
        registry.emplace<Textures>(entity, textures);
    }
};

#define SER_TEXTURES SOBJ(o.textures);
EXT_SER(Textures, SER_TEXTURES)
EXT_DES(Textures, SER_TEXTURES)

}  // namespace ecs

EXT_FMT(ecs::MapIcon, "tex: {}, size: {}", o.texHandle, o.size);
EXT_FMT(ecs::Texture,
        "tex: {}, bounds: {}, zIndex: {}, flags: {}, rot: {}",
        o.texHandle,
        o.bounds,
        o.zIndex,
        o.flags,
        o.rot);
EXT_FMT(ecs::Textures, "textures: {}", o.textures);

#endif