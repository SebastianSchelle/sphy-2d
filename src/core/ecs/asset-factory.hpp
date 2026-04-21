#ifndef ASSET_FACTORY_HPP
#define ASSET_FACTORY_HPP

#include <components/comp-phy.hpp>
#include <std-inc.hpp>

namespace mod
{
class ResourceMap;
}

namespace ecs
{
class Ecs;
struct EntityId;

class ComponentFactory
{
  public:
    using AssetLoaderFunc = std::function<void(entt::registry&,
                                               entt::entity,
                                               const YAML::Node&,
                                               mod::ResourceMap&)>;
    using AssetCopierFunc = std::function<void(const entt::registry&,
                                               entt::entity,
                                               entt::registry&,
                                               entt::entity)>;
    using DeserializeIntoRegistryFunc =
        std::function<void(entt::registry&,
                           entt::entity,
                           bitsery::Deserializer<InputAdapter>&)>;
    using SerializeFromRegistryFunc =
        std::function<void(entt::registry&,
                           entt::entity,
                           bitsery::Serializer<OutputAdapter>&)>;

    struct ComponentHelper
    {
        string name;
        AssetLoaderFunc assetLoader;
        AssetCopierFunc assetCopier;
        DeserializeIntoRegistryFunc deserializeIntoRegistry;
        SerializeFromRegistryFunc serializeFromRegistry;
    };

    void registerHelper(const std::string& name, ComponentHelper func);
    void loadComponent(const std::string& name,
                       entt::registry& registry,
                       entt::entity e,
                       const YAML::Node& node,
                       mod::ResourceMap& resourceMap);

    template <typename Component> void registerComponent()
    {
        const std::string name = Component::NAME;
        const uint32_t hash = hashConst(name.c_str());
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
                        // LG_D(
                        //     "Copying component: {} from asset entity: {} "
                        //     "to entity: {} value: {}",
                        //     name,
                        //     srcEntity,
                        //     dstEntity,
                        //     *component);
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
                },
                [name, hash](entt::registry& registry,
                             entt::entity entity,
                             bitsery::Serializer<OutputAdapter>& s)
                {
                    auto component = registry.try_get<Component>(entity);
                    if (component)
                    {
                        s.value4b(hash);
                        s.object(*component);
                    }
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
    entt::entity loadAsset(const std::string& path,
                           mod::ResourceMap& resourceMap);
    void copyComponentsIntoEntity(entt::registry& registry,
                                  entt::entity entity,
                                  const std::string& assetId) const;
    ecs::EntityId createFromAsset(ecs::Ecs& ecs, const std::string& assetId) const;
    string assetList(const string& assetId) const;
    string assetInfo(const string& assetId) const;
    bool hasAsset(const std::string& assetId) const;

  private:
    entt::registry registry;
    std::unordered_map<std::string, AssetMapItem> assetMap;
};

}  // namespace ecs

#endif