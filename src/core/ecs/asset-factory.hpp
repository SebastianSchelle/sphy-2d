#ifndef ASSET_FACTORY_HPP
#define ASSET_FACTORY_HPP

#include <components/comp-phy.hpp>
#include <std-inc.hpp>

namespace ecs
{

class ComponentFactory
{
  public:
    using AssetLoaderFunc =
        std::function<void(entt::registry&, entt::entity, const YAML::Node&)>;
    using AssetCopierFunc = std::function<void(const entt::registry&,
                                               entt::entity,
                                               entt::registry&,
                                               entt::entity)>;
    using DeserializeIntoRegistryFunc =
        std::function<void(entt::registry&,
                           entt::entity,
                           bitsery::Deserializer<InputAdapter>&)>;

    struct ComponentHelper
    {
        string name;
        AssetLoaderFunc assetLoader;
        AssetCopierFunc assetCopier;
        DeserializeIntoRegistryFunc deserializeIntoRegistry;
    };

    void registerHelper(const std::string& name, ComponentHelper func);
    void loadComponent(const std::string& name,
                       entt::registry& registry,
                       entt::entity e,
                       const YAML::Node& node);

    template <typename Component> void registerComponent()
    {
        const std::string name = Component::NAME;
        registerHelper(
            name,
            ComponentHelper(
                name,
                &Component::fromYaml,
                [name](const entt::registry& srcRegistry,
                       entt::entity srcEntity,
                       entt::registry& dstRegistry,
                       entt::entity dstEntity)
                {
                    auto component = srcRegistry.try_get<Component>(srcEntity);
                    if (component)
                    {
                        LG_D(
                            "Copying component: {} from asset entity: {} "
                            "to entity: {} value: {}",
                            name,
                            srcEntity,
                            dstEntity,
                            *component);
                        dstRegistry.emplace_or_replace<Component>(dstEntity,
                                                                  *component);
                    }
                },
                [name](entt::registry& registry,
                       entt::entity entity,
                       bitsery::Deserializer<InputAdapter>& s)
                {
                    Component component;
                    s.object(component);
                    registry.emplace_or_replace<Component>(entity, component);
                }));
    }

    const std::unordered_map<uint32_t, ComponentHelper>&
    getComponentHelpers() const
    {
        return componentHelpers;
    }

  private:
    std::unordered_map<uint32_t, ComponentHelper> componentHelpers;
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