#include <lib-modules.hpp>

namespace gobj
{

namespace mdata
{

namespace
{

constexpr const char* kStorageVolumeYamlKeys[static_cast<size_t>(
    StorageType::NumStorageTypes)] = {
    "volume-container-s",
    "volume-container-l",
    "volume-tank",
    "volume-bulk",
};

}  // namespace

MainThruster MainThruster::fromYaml(const YAML::Node& node)
{
    MainThruster mainThruster;
    TRY_YAML_DICT(mainThruster.maxThrust, node["max-thrust"], 100.0f);
    return mainThruster;
}

ManeuverThruster ManeuverThruster::fromYaml(const YAML::Node& node)
{
    ManeuverThruster maneuverThruster;
    TRY_YAML_DICT(maneuverThruster.maxThrust, node["max-thrust"], 100.0f);
    return maneuverThruster;
}

Storage Storage::fromYaml(const YAML::Node& node)
{
    Storage storage{};
    for (size_t i = 0; i < static_cast<size_t>(StorageType::NumStorageTypes); ++i)
    {
        storage.volume[i] = 0.0f;
    }

    bool anyPerType = false;
    for (size_t i = 0; i < static_cast<size_t>(StorageType::NumStorageTypes); ++i)
    {
        if (node[kStorageVolumeYamlKeys[i]])
        {
            anyPerType = true;
            float parsed = 0.0f;
            TRY_YAML_DICT(parsed, node[kStorageVolumeYamlKeys[i]], 0.0f);
            storage.volume[i] = parsed;
        }
    }

    return storage;
}

Hangar Hangar::fromYaml(const YAML::Node& node)
{
    Hangar hangar;
    return hangar;
}

}  // namespace mdata

Module Module::fromYaml(const YAML::Node& node,
                        const con::ItemLib<gobj::Textures>& texturesLib)
{
    Module module;
    TRY_YAML_DICT(module.name, node["name"], "");
    TRY_YAML_DICT(module.description, node["description"], "");
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        module.textures = texturesLib.getHandle(texturesName);
    }
    string slotTypeStr = "";
    TRY_YAML_DICT(slotTypeStr, node["slot-type"], "");
    module.slotType =
        magic_enum::enum_cast<ModuleSlotType>(slotTypeStr).value();
    string typeStr = "";
    TRY_YAML_DICT(typeStr, node["type"], "");
    module.type = magic_enum::enum_cast<ModuleType>(typeStr).value();

    const auto& dataNode = node["data"];
    if (dataNode && dataNode.IsMap())
    {
        switch (module.type)
        {
            case ModuleType::MainThruster:
                module.data = mdata::MainThruster::fromYaml(dataNode);
                break;
            case ModuleType::ManeuverThruster:
                module.data = mdata::ManeuverThruster::fromYaml(dataNode);
                break;
            case ModuleType::Storage:
                module.data = mdata::Storage::fromYaml(dataNode);
                break;
            case ModuleType::Hangar:
                module.data = mdata::Hangar::fromYaml(dataNode);
                break;
            default:
                break;
        }
    }
    return module;
}

}  // namespace gobj
