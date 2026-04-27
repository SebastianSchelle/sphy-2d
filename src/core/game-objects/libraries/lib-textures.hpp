#ifndef LIB_TEXTURES_HPP
#define LIB_TEXTURES_HPP

#include <item-lib.hpp>
#include <magic_enum/magic_enum.hpp>
#include <std-inc.hpp>

namespace mod
{
class ResourceMap;
}

namespace gobj
{

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
    TextureFlags flags;
    int8_t zIndex;
};

struct Textures
{
    std::string name;
    std::string description;
    std::vector<Texture> textures;

    static Textures fromYaml(const YAML::Node& node,
                             mod::ResourceMap& resourceMap);
};

using TexturesHandle = typename con::ItemLib<Textures>::Handle;

struct MapIcon
{
    std::string name;
    std::string description;
    GenericHandle texHandle;
    vec2 size;

    static MapIcon fromYaml(const YAML::Node& node,
                            mod::ResourceMap& resourceMap);
};

using MapIconHandle = typename con::ItemLib<MapIcon>::Handle;

}  // namespace gobj

EXT_FMT(gobj::TextureFlags, "{}", magic_enum::enum_name(o));

EXT_FMT(gobj::Texture,
        "(texHandle: {}, bounds: {}, rot: {}, flags: {}, zIndex: {})",
        o.texHandle,
        o.bounds,
        o.rot,
        o.flags,
        o.zIndex);

EXT_FMT(gobj::Textures,
        "(name: {}, description: {}, textures: {})",
        o.name,
        o.description,
        o.textures);

EXT_FMT(gobj::MapIcon,
        "(name: {}, description: {}, texHandle: {}, size: {})",
        o.name,
        o.description,
        o.texHandle,
        o.size);

#endif  // LIB_HULL_HPP