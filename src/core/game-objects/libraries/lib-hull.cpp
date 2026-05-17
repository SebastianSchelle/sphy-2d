#include <lib-hull.hpp>

namespace gobj
{

namespace
{

constexpr const char* kHullVolumeYamlKeys[static_cast<size_t>(
    StorageType::NumStorageTypes)] = {
    "volume-container-s",
    "volume-container-l",
    "volume-tank",
    "volume-bulk",
};

}  // namespace

Hull Hull::fromYaml(const YAML::Node& node,
                    const con::ItemLib<gobj::Textures>& texturesLib,
                    const con::ItemLib<gobj::Collider>& colliderLib)
{
    Hull hull;
    TRY_YAML_DICT(hull.name, node["name"], "");
    TRY_YAML_DICT(hull.description, node["description"], "");
    TRY_YAML_DICT(hull.hullpoints, node["hullpoints"], 100.0f);
    TRY_YAML_DICT(hull.mass, node["mass"], 1.0f);
    TRY_YAML_DICT(hull.size.x, node["size"][0], 0.0f);
    TRY_YAML_DICT(hull.size.y, node["size"][1], 0.0f);
    string shipClassStr = "Drone";
    TRY_YAML_DICT(shipClassStr, node["ship-class"], "Drone");
    hull.shipClass =
        magic_enum::enum_cast<ShipClass>(shipClassStr)
            .value_or(ShipClass::Drone);
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        hull.textures = texturesLib.getHandle(texturesName);
    }
    string colliderName = "";
    TRY_YAML_DICT(colliderName, node["collider"], "");
    if (colliderName != "")
    {
        hull.collider = colliderLib.getHandle(colliderName);
    }
    for (size_t i = 0; i < static_cast<size_t>(StorageType::NumStorageTypes); ++i)
    {
        if (node[kHullVolumeYamlKeys[i]])
        {
            float parsed = 0.0f;
            TRY_YAML_DICT(parsed, node[kHullVolumeYamlKeys[i]], 0.0f);
            hull.volume[i] = parsed;
        }
    }
    if (node["slots"])
    {
        for (const auto& slotNode : node["slots"])
        {
            ModuleSlot slot;
            std::string modTypeStr;
            TRY_YAML_DICT(
                modTypeStr, slotNode["mod-type"], "ThrusterMainS_Common");
            slot.type =
                magic_enum::enum_cast<ModuleSlotType>(modTypeStr).value();
            TRY_YAML_DICT(slot.pos, slotNode["pos"], vec2(0.0f, 0.0f));
            float rotDeg = 0.0f;
            TRY_YAML_DICT(rotDeg, slotNode["rot"], 0.0f);
            slot.rot = smath::degToRad(rotDeg);
            hull.slots.push_back(slot);
        }
    }
    return hull;
}

}  // namespace gobj
