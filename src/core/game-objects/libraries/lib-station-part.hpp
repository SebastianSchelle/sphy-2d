#ifndef LIB_STATION_PART_HPP
#define LIB_STATION_PART_HPP

#include <lib-textures.hpp>
#include <lib-collider.hpp>
#include <std-inc.hpp>

namespace gobj
{

constexpr float kConnectorWidth = 60.0f;
constexpr float kConnectorHeight = 2.63f;

enum class StationPartType : uint8_t
{
    Structural,
    Storage,
    Docking,
    Habitat,
    Defense,
    Production,
    LifeSupport,
};

namespace sdata
{

struct Structural
{
    static Structural fromYaml(const YAML::Node& node);
};

struct Storage
{
    float capContainerS;
    float capContainerL;
    float capTank;
    float capBulk;
    static Storage fromYaml(const YAML::Node& node);
};

struct Docking
{
    static Docking fromYaml(const YAML::Node& node);
};

struct Habitat
{
    static Habitat fromYaml(const YAML::Node& node);
};

struct Defense
{
    static Defense fromYaml(const YAML::Node& node);
};

struct Production
{
    static Production fromYaml(const YAML::Node& node);
};

struct LifeSupport
{
    static LifeSupport fromYaml(const YAML::Node& node);
};

using Data = std::variant<Structural,
                          Storage,
                          Docking,
                          Habitat,
                          Defense,
                          Production,
                          LifeSupport>;
}

struct Connector
{
    vec2 pos;
    float rot;
};

struct StationPart
{
    string name;
    string description;
    StationPartType type;
    float hp;
    TexturesHandle textures = TexturesHandle::Invalid();
    ColliderHandle collider = ColliderHandle::Invalid();
    std::vector<Connector> connectors;
    sdata::Data data;

    static StationPart fromYaml(const YAML::Node& node,
                                const con::ItemLib<gobj::Textures>& texturesLib,
                                const con::ItemLib<gobj::Collider>& colliderLib);
};
using StationPartHandle = typename con::ItemLib<StationPart>::Handle;

}  // namespace gobj

EXT_FMT(gobj::StationPartType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::StationPart,
        "(name: {}, description: {}, type: {}, hp: {}, textures: {}, collider: {})",
        o.name,
        o.description,
        o.type,
        o.hp,
        o.textures.toString(),
        o.collider.toString());

#endif
