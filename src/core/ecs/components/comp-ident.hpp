#ifndef COMP_IDENT_HPP
#define COMP_IDENT_HPP

#include <ecs.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace ecs
{

struct AssetId
{
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

struct SectorId
{
    uint32_t id;
    static void fromYaml(entt::registry& registry, entt::entity entity, const YAML::Node& node)
    {
        SectorId sectorId;
        sectorId.id = node["id"].as<uint32_t>();
        registry.emplace<SectorId>(entity, sectorId);
    }
};

}  // namespace ecs



template <> struct fmt::formatter<ecs::AssetId>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const ecs::AssetId& assetId, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", assetId.name);
    }
};

template <> struct fmt::formatter<ecs::SectorId>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const ecs::SectorId& sectorId, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", sectorId.id);
    }
};

#endif