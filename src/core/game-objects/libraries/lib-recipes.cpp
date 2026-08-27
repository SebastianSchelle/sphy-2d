#include "lib-modules.hpp"
#include <lib-recipes.hpp>

namespace gobj
{

gobj::HullHandle resolveHullHandle(const string& key,
                                   const con::ItemLib<gobj::Hull>& hullLib)
{
    if (key.empty())
    {
        LG_E("Hull key is empty");
        return gobj::HullHandle::Invalid();
    }
    auto handle = hullLib.getHandle(key);
    if (!handle.isValid())
    {
        LG_E("Hull {} not found", key);
        return gobj::HullHandle::Invalid();
    }
    return handle;
}

gobj::ModuleHandle
resolveModuleHandle(const string& key,
                    const con::ItemLib<gobj::Module>& moduleLib)
{
    if (key.empty())
    {
        LG_E("Module key is empty");
        return gobj::ModuleHandle::Invalid();
    }
    auto handle = moduleLib.getHandle(key);
    if (!handle.isValid())
    {
        LG_E("Module {} not found", key);
        return gobj::ModuleHandle::Invalid();
    }
    return handle;
}

ShipRecipe ShipRecipe::fromYaml(const YAML::Node& node,
                                con::ItemLib<gobj::Hull>& hullLib,
                                con::ItemLib<gobj::Module>& moduleLib)
{
    ShipRecipe recipe;
    TRY_YAML_DICT(recipe.name, node["name"], "");
    TRY_YAML_DICT(recipe.description, node["description"], "");
    string hullKey;
    TRY_YAML_DICT(hullKey, node["hull"], "");
    recipe.hullHandle = resolveHullHandle(hullKey, hullLib);
    if (node["slots"])
    {
        uint i = 0;
        for (const auto& slotNode : node["slots"])
        {
            string mod;
            string modId = slotNode.as<string>();
            if (modId == "None")
            {
                i++;
                continue;
            }
            LG_D("add slot {}:{}", i, modId);
            auto handle = resolveModuleHandle(modId, moduleLib);
            recipe.modSlot.push_back({.slot = i, .modHandle = handle});
            i++;
        }
    }
    return recipe;
}


}  // namespace gobj
