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
    ModuleSlotType modType;
    vec2 pos;
    float rot;
    uint8_t z;
};

struct ThrusterManeuver
{
    string name;
    string description;
    float maxThrust;
    TexturesHandle textures = TexturesHandle::Invalid();

    static ThrusterManeuver
    fromYaml(const YAML::Node& node,
             const con::ItemLib<gobj::Textures>& texturesLib);
};

using ThrusterManeuverHandle = typename con::ItemLib<ThrusterManeuver>::Handle;

struct ThrusterMain
{
    string name;
    string description;
    float maxThrust;
    TexturesHandle textures = TexturesHandle::Invalid();

    static ThrusterMain
    fromYaml(const YAML::Node& node,
             const con::ItemLib<gobj::Textures>& texturesLib);
};

using ThrusterMainHandle = typename con::ItemLib<ThrusterMain>::Handle;

}  // namespace gobj

EXT_FMT(gobj::ModuleSlotType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::ModuleSlot,
        "(modType: {}, pos: {}, rot: {}, z: {})",
        o.modType,
        o.pos,
        o.rot,
        o.z);

EXT_FMT(gobj::ThrusterManeuver,
        "(name: {}, description: {}, maxThrust: {}, textures: {})",
        o.name,
        o.description,
        o.maxThrust,
        o.textures.toString());
EXT_FMT(gobj::ThrusterMain,
        "(name: {}, description: {}, maxThrust: {}, textures: {})",
        o.name,
        o.description,
        o.maxThrust,
        o.textures.toString());

#endif  // LIB_MODULES_HPP