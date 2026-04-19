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

    uint16_t texIdx;
    uint16_t texGen;
    vec2 size;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        MapIcon mapIcon;
        string texName = node["texName"].as<string>();
        mod::MappedTextureHandle mTexHandle = resourceMap.getTextureHandle(texName);
        if (mTexHandle.isValid())
        {
            mapIcon.texIdx = mTexHandle.getIdx();
            mapIcon.texGen = mTexHandle.getGeneration();
        }
        else
        {
            LG_E("Texture not found: {}", texName);
        }
        TRY_YAML_DICT(mapIcon.size, node["size"], vec2(10.0f, 10.0f));
        registry.emplace<MapIcon>(entity, mapIcon);
    }
};

#define SER_MAP_ICON                                                           \
    S2b(o.texIdx);                                                \
    S2b(o.texGen);                                         \
    SOBJ(o.size);
EXT_SER(MapIcon, SER_MAP_ICON)
EXT_DES(MapIcon, SER_MAP_ICON)

}  // namespace ecs

EXT_FMT(ecs::MapIcon,
        "{}:{}",
        o.texIdx,
        o.texGen);

#endif