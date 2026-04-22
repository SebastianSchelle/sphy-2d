#ifndef COMP_IDENT_HPP
#define COMP_IDENT_HPP

#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>
#include <entt/entt.hpp>

namespace mod
{
class ResourceMap;
}

namespace ecs
{

struct EntityId
{
    uint32_t index;
    uint16_t generation;
    bool operator==(const EntityId& other) const
    {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const EntityId& other) const
    {
        return !(*this == other);
    }
    static const EntityId Invalid()
    {
        return {0, 0};
    }
};

#define SER_ENTITY_ID S4b(o.index); S2b(o.generation);
EXT_SER(EntityId, SER_ENTITY_ID)
EXT_DES(EntityId, SER_ENTITY_ID)

struct AssetId
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "asset-id";

    std::string name;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        AssetId assetId;
        assetId.name = node["name"].as<std::string>();
        registry.emplace<AssetId>(entity, assetId);
    }
};

// Bitsery: strings use text1b, not value4b (fundamental types only).
#define SER_ASSET_ID s.text1b(o.name, 512);
EXT_SER(AssetId, SER_ASSET_ID)
EXT_DES(AssetId, SER_ASSET_ID)


struct SectorId
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "sector-id";

    uint32_t id;
    uint32_t x;
    uint32_t y;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        SectorId sectorId;
        TRY_YAML_DICT(sectorId.id, node["id"], 0u);
        TRY_YAML_DICT(sectorId.x, node["x"], 0u);
        TRY_YAML_DICT(sectorId.y, node["y"], 0u);
        registry.emplace<SectorId>(entity, sectorId);
    }

    vec2 toVec2() const
    {
        return vec2(x, y);
    }
};

#define SER_SECTOR_ID                                                          \
    S4b(o.id);                                                                 \
    S4b(o.x);                                                                  \
    S4b(o.y);
EXT_SER(SectorId, SER_SECTOR_ID)
EXT_DES(SectorId, SER_SECTOR_ID)

}  // namespace ecs

EXT_FMT(ecs::EntityId, "{}:{}", o.index, o.generation);
EXT_FMT(ecs::AssetId, "{}", o.name);
EXT_FMT(ecs::SectorId, "s:{}, x:{}, y:{}", o.id, o.x, o.y);

#endif