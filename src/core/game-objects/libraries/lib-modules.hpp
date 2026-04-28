#ifndef LIB_MODULES_HPP
#define LIB_MODULES_HPP

#include <lib-textures.hpp>
#include <magic_enum/magic_enum.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace gobj
{

enum class ModuleSlotType : uint8_t
{
    ThrusterMainS_Common,
    ThrusterMainM_Common,
    ThrusterMainL_Common,
    ThrusterManeuverS_Common,
    ThrusterManeuverM_Common,
    ThrusterManeuverL_Common,
    InternalS_Common,
    InternalM_Common,
    InternalL_Common,
    RoofS_Common,
    RoofM_Common,
    RoofL_Common,
    BayS_Common,
    BayM_Common,
    BayL_Common,
};

struct ModuleSlot
{
    ModuleSlotType type;
    vec2 pos;
    float rot;
    uint8_t z;
};

enum class ModuleType : uint8_t
{
    MainThruster,
    ManeuverThruster,
    Storage,
};

namespace mdata
{

struct MainThruster
{
    float maxThrust;
    static MainThruster fromYaml(const YAML::Node& node);
};
struct ManeuverThruster
{
    float maxThrust;
    static ManeuverThruster fromYaml(const YAML::Node& node);
};
struct Storage
{
    float volume;
    static Storage fromYaml(const YAML::Node& node);
};

using Data = std::variant<MainThruster, ManeuverThruster, Storage>;

}  // namespace mdata

struct Module
{
    string name;
    string description;
    ModuleSlotType slotType;
    TexturesHandle textures = TexturesHandle::Invalid();
    ModuleType type;
    mdata::Data data;

    static Module fromYaml(const YAML::Node& node,
                           const con::ItemLib<gobj::Textures>& texturesLib);
};

using ModuleHandle = typename con::ItemLib<Module>::Handle;

}  // namespace gobj

EXT_FMT(gobj::ModuleSlotType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::ModuleSlot,
        "(modType: {}, pos: {}, rot: {}, z: {})",
        o.type,
        o.pos,
        o.rot,
        o.z);
EXT_FMT(gobj::ModuleType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::Module,
        "(name: {}, description: {}, type: {}, slotType: {}, textures: {})",
        o.name,
        o.description,
        o.type,
        o.slotType,
        o.textures.toString());

#endif  // LIB_MODULES_HPP