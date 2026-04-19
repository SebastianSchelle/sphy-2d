#include <asset-factory.hpp>
#include <components/comp-ident.hpp>

namespace ecs
{

void ComponentFactory::registerHelper(const std::string& name,
                                      ComponentHelper func)
{
    componentHelpers[hashConst(name.c_str())] = func;
}

void ComponentFactory::loadComponent(const std::string& name,
                                     entt::registry& registry,
                                     entt::entity e,
                                     const YAML::Node& node,
                                     mod::ResourceMap& resourceMap)
{
    auto it = componentHelpers.find(hashConst(name.c_str()));
    if (it != componentHelpers.end())
    {
        it->second.assetLoader(registry, e, node, resourceMap);
    }
    else
    {
        throw std::runtime_error("Unknown component: " + name);
    }
}

AssetFactory::AssetFactory() {}

AssetFactory::~AssetFactory() {}

entt::entity AssetFactory::loadAsset(const std::string& path,
                                     mod::ResourceMap& resourceMap)
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
                    assetMap[name] = {path, asset};
                    LG_I("Loaded asset '{}' from '{}'", name, path);
                }
                componentFactory.loadComponent(
                    component["type"].as<std::string>(),
                    registry,
                    asset,
                    component,
                    resourceMap);
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
    const entt::entity srcEntity = it->second.entity;
    if (!registry.valid(entity) || !this->registry.valid(srcEntity))
    {
        return;
    }
    for (const auto& [hash, helper] : componentFactory.getComponentHelpers())
    {
        if (helper.name == "asset-id")
        {
            continue;
        }
        helper.assetCopier(const_cast<entt::registry&>(this->registry),
                           srcEntity,
                           registry,
                           entity);
    }
}

string AssetFactory::assetList(const string& searchTerm) const
{
    string info = "List of available assets:\n";
    for (const auto& [name, entity] : assetMap)
    {
        if (searchTerm.empty() || searchTerm == "all"
            || name.find(searchTerm) != string::npos)
        {
            info += name + "\n";
        }
    }
    return info;
}

string AssetFactory::assetInfo(const string& assetId) const
{
    auto it = assetMap.find(assetId);
    if (it == assetMap.end())
    {
        return "Failed: Asset not found";
    }
    const AssetMapItem& item = it->second;
    string info = "Asset '" + assetId + "'";
    std::ifstream file(item.path);
    if (file.is_open())
    {
        string line;
        while (getline(file, line))
        {
            info += line + "\n";
        }
        return info;
    }
    else
    {
        return "Failed: Could not find asset info";
    }
}

}  // namespace ecs
