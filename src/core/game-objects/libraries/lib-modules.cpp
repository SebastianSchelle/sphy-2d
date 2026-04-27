#include <lib-modules.hpp>

namespace gobj
{

ThrusterManeuver ThrusterManeuver::fromYaml(const YAML::Node& node,
                                            const con::ItemLib<gobj::Textures>& texturesLib)
{
    ThrusterManeuver thrusterManeuver;
    TRY_YAML_DICT(thrusterManeuver.name, node["name"], "");
    TRY_YAML_DICT(thrusterManeuver.description, node["description"], "");
    TRY_YAML_DICT(thrusterManeuver.maxThrust, node["max-thrust"], 100.0f);
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        thrusterManeuver.textures = texturesLib.getHandle(texturesName);
    }
    return thrusterManeuver;
}

ThrusterMain ThrusterMain::fromYaml(const YAML::Node& node,
                                    const con::ItemLib<gobj::Textures>& texturesLib)
{
    ThrusterMain thrusterMain;
    TRY_YAML_DICT(thrusterMain.name, node["name"], "");
    TRY_YAML_DICT(thrusterMain.description, node["description"], "");
    TRY_YAML_DICT(thrusterMain.maxThrust, node["max-thrust"], 100.0f);
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        thrusterMain.textures = texturesLib.getHandle(texturesName);
    }
    return thrusterMain;
}

}  // namespace gobj
