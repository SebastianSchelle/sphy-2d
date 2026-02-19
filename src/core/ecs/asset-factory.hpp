#ifndef ASSET_FACTORY_HPP
#define ASSET_FACTORY_HPP

#include <components/comp-phy.hpp>
#include <ecs.hpp>
#include <std-inc.hpp>

namespace ecs
{

class ComponentFactory
{
  public:
    using LoaderFunc =
        std::function<void(entt::registry&, entt::entity, const YAML::Node&)>;

    void registerLoader(const std::string& name, LoaderFunc func);
    void loadComponent(const std::string& name,
                      entt::registry& registry,
                      entt::entity e,
                      const YAML::Node& node);

    template <typename Component>
    void registerComponent(const std::string& name)
    {
        registerLoader(name, &Component::fromYaml);
    }

  private:
    std::unordered_map<std::string, LoaderFunc> loaders;
};

class AssetFactory
{
  public:
    AssetFactory();
    ~AssetFactory();
    ComponentFactory componentFactory;
    entt::entity loadAsset(const std::string& path);
  private:
    entt::registry registry;
    std::unordered_map<std::string, entt::entity> assetMap;
};

}  // namespace ecs

#endif