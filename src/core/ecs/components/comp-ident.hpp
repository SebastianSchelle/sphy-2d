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

}  // namespace ecs

#endif