#ifndef COMP_IDENT_HPP
#define COMP_IDENT_HPP

#include <ecs.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace ecs
{

struct AssetId
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "asset-id";

    std::string name;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node)
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

    static void fromYaml(entt::registry& registry, entt::entity entity, const YAML::Node& node)
    {
        SectorId sectorId;
        TRY_YAML_DICT(sectorId.id, node["id"], 0u);
        TRY_YAML_DICT(sectorId.x, node["x"], 0u);
        TRY_YAML_DICT(sectorId.y, node["y"], 0u);
        registry.emplace<SectorId>(entity, sectorId);
    }
};

#define SER_SECTOR_ID                                                           \
    S4b(o.id);                                                               \
    S4b(o.x);                                                                 \
    S4b(o.y);
EXT_SER(SectorId, SER_SECTOR_ID)
EXT_DES(SectorId, SER_SECTOR_ID)

}  // namespace ecs

EXT_FMT(ecs::AssetId, "{}", o.name);
EXT_FMT(ecs::SectorId, "s:{}, x:{}, y:{}", o.id, o.x, o.y);

#endif