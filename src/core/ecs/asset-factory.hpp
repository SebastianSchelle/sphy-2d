#ifndef ASSET_FACTORY_HPP
#define ASSET_FACTORY_HPP

#include <components/comp-phy.hpp>
#include <std-inc.hpp>

namespace ecs
{

class ComponentFactory
{
  public:
    using LoaderFunc =
        std::function<void(entt::registry&, entt::entity, const YAML::Node&)>;
    using CopierFunc = std::function<void(const entt::registry&,
                                          entt::entity,
                                          entt::registry&,
                                          entt::entity)>;

    void registerLoader(const std::string& name, LoaderFunc func);
    void registerCopier(const std::string& name, CopierFunc func);
    void loadComponent(const std::string& name,
                       entt::registry& registry,
                       entt::entity e,
                       const YAML::Node& node);

    template <typename Component>
    void registerComponent(const std::string& name)
    {
        registerLoader(name, &Component::fromYaml);
        registerCopier(
            name,
            [name](const entt::registry& srcRegistry,
               entt::entity srcEntity,
               entt::registry& dstRegistry,
               entt::entity dstEntity)
            {
                auto component = srcRegistry.try_get<Component>(srcEntity);
                if (component)
                {
                    LG_D("Copying component: {} from entity: {} to entity: {} value: {}",
                         name,
                         srcEntity,
                         dstEntity, *component);
                    dstRegistry.emplace_or_replace<Component>(dstEntity,
                                                              *component);
                }
            });
    }

    const std::unordered_map<std::string, CopierFunc>& getCopiers() const
    {
        return copiers;
    }

  private:
    std::unordered_map<std::string, LoaderFunc> loaders;
    std::unordered_map<std::string, CopierFunc> copiers;
};

struct AssetMapItem
{
  std::string path;
  entt::entity entity;
};

class AssetFactory
{
  public:
    AssetFactory();
    ~AssetFactory();
    ComponentFactory componentFactory;
    entt::entity loadAsset(const std::string& path);
    void copyComponentsIntoEntity(entt::registry& registry,
                                  entt::entity entity,
                                  const std::string& assetId) const;
    string assetList(const string& assetId) const;
    string assetInfo(const string& assetId) const;

  private:
    entt::registry registry;
    std::unordered_map<std::string, AssetMapItem> assetMap;
};

}  // namespace ecs

#endif