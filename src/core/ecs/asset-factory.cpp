#include <asset-factory.hpp>

namespace ecs
{

void ComponentFactory::registerLoader(const std::string& name, LoaderFunc func)
{
    loaders[name] = func;
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
                    const std::string name = component["name"].as<std::string>();
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

}  // namespace ecs
