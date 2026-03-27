#include <asset-factory.hpp>
#include <components/comp-ident.hpp>

namespace ecs
{

void ComponentFactory::registerLoader(const std::string& name, LoaderFunc func)
{
    loaders[name] = func;
}

void ComponentFactory::registerCopier(const std::string& name, CopierFunc func)
{
    copiers[name] = func;
}

void ComponentFactory::loadComponent(const std::string& name,
                                     entt::registry& registry,
                                     entt::entity e,
                                     const YAML::Node& node)
{
    auto it = loaders.find(name);
    if (it != loaders.end())
    {
        it->second(registry, e, node);
    }
    else
    {
        throw std::runtime_error("Unknown component: " + name);
    }
}

AssetFactory::AssetFactory() {}

AssetFactory::~AssetFactory() {}

entt::entity AssetFactory::loadAsset(const std::string& path)
{
    YAML::Node node = YAML::LoadFile(path);
    entt::entity asset = entt::null;
    if (node.IsSequence())
    {
        asset = registry.create();
        for (auto component : node)
        {
            try
            {
                if (component["type"].as<std::string>() == "asset-id")
                {
                    const std::string name =
                        component["name"].as<std::string>();
                    assetMap[name] = asset;
                    LG_I("Loaded asset '{}' from '{}'", name, path);
                }
                componentFactory.loadComponent(
                    component["type"].as<std::string>(),
                    registry,
                    asset,
                    component);
            }
            catch (const std::runtime_error& e)
            {
                LG_E("Error loading asset component: {}", e.what());
            }
        }
    }
    return asset;
}

void AssetFactory::copyComponentsIntoEntity(entt::registry& registry,
                                            entt::entity entity,
                                            const std::string& assetId) const
{
    auto it = assetMap.find(assetId);
    if (it == assetMap.end())
    {
        return;
    }
    const entt::entity srcEntity = it->second;
    if (!registry.valid(entity) || !this->registry.valid(srcEntity))
    {
        return;
    }
    for (const auto& [name, copier] : componentFactory.getCopiers())
    {
        copier(const_cast<entt::registry&>(this->registry),
               srcEntity,
               registry,
               entity);
    }
}

}  // namespace ecs
