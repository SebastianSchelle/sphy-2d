#include <lib-hull.hpp>

namespace gobj
{

Hull Hull::fromYaml(const YAML::Node& node,
                    const con::ItemLib<gobj::Textures>& texturesLib,
                    const con::ItemLib<gobj::Collider>& colliderLib,
                    const con::ItemLib<gobj::MapIcon>& mapIconLib)
{
    Hull hull;
    TRY_YAML_DICT(hull.name, node["name"], "");
    TRY_YAML_DICT(hull.description, node["description"], "");
    TRY_YAML_DICT(hull.hullpoints, node["hullpoints"], 100.0f);
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
    string mapIconName = "";
    TRY_YAML_DICT(mapIconName, node["map-icon"], "");
    if (mapIconName != "")
    {
        hull.mapIcon = mapIconLib.getHandle(mapIconName);
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
            TRY_YAML_DICT(slot.z, slotNode["z"], uint8_t(0));
            hull.slots.push_back(slot);
        }
    }
    return hull;
}

}  // namespace gobj