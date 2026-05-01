#include <lib-station-part.hpp>

namespace gobj
{


namespace sdata
{

Structural Structural::fromYaml(const YAML::Node& node)
{
    Structural structural;
    return structural;
}

Storage Storage::fromYaml(const YAML::Node& node)
{
    Storage storage;
    TRY_YAML_DICT(storage.capContainerS, node["cap-container-s"], 0.0f);
    TRY_YAML_DICT(storage.capContainerL, node["cap-container-l"], 0.0f);
    TRY_YAML_DICT(storage.capTank, node["cap-tank"], 0.0f);
    TRY_YAML_DICT(storage.capBulk, node["cap-bulk"], 0.0f);
    return storage;
}

Docking Docking::fromYaml(const YAML::Node& node)
{
    Docking docking;
    return docking;
}

Habitat Habitat::fromYaml(const YAML::Node& node)
{
    Habitat habitat;
    return habitat;
}

Defense Defense::fromYaml(const YAML::Node& node)
{
    Defense defense;
    return defense;
}

Production Production::fromYaml(const YAML::Node& node)
{
    Production production;
    return production;
}

LifeSupport LifeSupport::fromYaml(const YAML::Node& node)
{
    LifeSupport lifeSupport;
    return lifeSupport;
}

}  // namespace sdata

StationPart StationPart::fromYaml(const YAML::Node& node,
                                 const con::ItemLib<gobj::Textures>& texturesLib,
                                 const con::ItemLib<gobj::Collider>& colliderLib)
{
    StationPart stationPart;
    TRY_YAML_DICT(stationPart.name, node["name"], "");
    TRY_YAML_DICT(stationPart.description, node["description"], "");
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        stationPart.textures = texturesLib.getHandle(texturesName);
    }
    string colliderName = "";
    TRY_YAML_DICT(colliderName, node["collider"], "");
    if (colliderName != "")
    {
        stationPart.collider = colliderLib.getHandle(colliderName);
    }
    string typeStr = "";
    TRY_YAML_DICT(typeStr, node["type"], "");
    stationPart.type = magic_enum::enum_cast<StationPartType>(typeStr).value();
    if (node["connectors"])
    {
        for (const auto& connectorNode : node["connectors"])
        {
            Connector connector;
            TRY_YAML_DICT(connector.pos, connectorNode["pos"], vec2(0.0f, 0.0f));
            float rotDeg = 0.0f;
            TRY_YAML_DICT(rotDeg, connectorNode["rot"], 0.0f);
            connector.rot = smath::degToRad(rotDeg);
            stationPart.connectors.push_back(connector);
        }
    }
    const auto& dataNode = node["data"];
    if (dataNode && dataNode.IsMap())
    {
        switch (stationPart.type)
        {
            case StationPartType::Structural:
                stationPart.data = sdata::Structural::fromYaml(dataNode);
                break;
            case StationPartType::Storage:
                stationPart.data = sdata::Storage::fromYaml(dataNode);
                break;
            case StationPartType::Docking:
                stationPart.data = sdata::Docking::fromYaml(dataNode);
                break;
            case StationPartType::Habitat:
                stationPart.data = sdata::Habitat::fromYaml(dataNode);
                break;
            case StationPartType::Defense:
                stationPart.data = sdata::Defense::fromYaml(dataNode);
                break;
            case StationPartType::Production:
                stationPart.data = sdata::Production::fromYaml(dataNode);
                break;
            case StationPartType::LifeSupport:
                stationPart.data = sdata::LifeSupport::fromYaml(dataNode);
                break;
            default:
                break;
        }
    }
    return stationPart;
}

}  // namespace gobj