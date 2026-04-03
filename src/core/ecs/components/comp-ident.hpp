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

    static void fromYaml(entt::registry& registry, entt::entity entity, const YAML::Node& node)
    {
        SectorId sectorId;
        sectorId.id = node["id"].as<uint32_t>();
        registry.emplace<SectorId>(entity, sectorId);
    }
};

#define SER_SECTOR_ID                                                           \
    S4b(o.id);
EXT_SER(SectorId, SER_SECTOR_ID)
EXT_DES(SectorId, SER_SECTOR_ID)

}  // namespace ecs

EXT_FMT(ecs::AssetId, "{}", o.name);
EXT_FMT(ecs::SectorId, "{}", o.id);

#endif