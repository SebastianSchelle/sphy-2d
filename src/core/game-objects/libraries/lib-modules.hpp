#ifndef LIB_MODULES_HPP
#define LIB_MODULES_HPP

#include <lib-textures.hpp>
#include <magic_enum/magic_enum.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>
#include <ship-def.hpp>

namespace gobj
{

enum class StorageType : uint8_t
{
    ContainerS,
    ContainerL,
    Tank,
    Bulk,
    NumStorageTypes,
};

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
    NumSlotTypes
};

constexpr int8_t
    ModuleSlotZOffset[static_cast<size_t>(ModuleSlotType::NumSlotTypes)] = {
        -5,  // ThrusterMainM_Common
        -5,  // ThrusterMainL_Common
        -5,  // ThrusterManeuverS_Common
        -5,  // ThrusterManeuverM_Common
        -5,  // ThrusterManeuverL_Common
        -5,  // InternalS_Common
        -5,  // InternalM_Common
        -5,  // InternalL_Common
        5,   // RoofS_Common
        5,   // RoofM_Common
        5,   // RoofL_Common
        -5,  // BayS_Common
        -5,  // BayM_Common
        -5,  // BayL_Common
};

struct ModuleSlot
{
    ModuleSlotType type;
    vec2 pos;
    float rot;
};

enum class ModuleType : uint8_t
{
    None,
    MainThruster,
    ManeuverThruster,
    Storage,
    Hangar,
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
    float volume[static_cast<size_t>(gobj::StorageType::NumStorageTypes)];
    static Storage fromYaml(const YAML::Node& node);
};
struct Hangar
{
    def::ShipClass maxShipClass;
    float hangarSpace;
    static Hangar fromYaml(const YAML::Node& node);
};
struct Turret
{
    static Turret fromYaml(const YAML::Node& node);
};
using Data =
    std::variant<MainThruster, ManeuverThruster, Storage, Hangar, Turret>;

}  // namespace mdata

struct Module
{
    string name;
    string description;
    ModuleSlotType slotType;
    TexturesHandle textures = TexturesHandle::Invalid();
    ModuleType type;
    float mass;
    mdata::Data data;

    static Module fromYaml(const YAML::Node& node,
                           const con::ItemLib<gobj::Textures>& texturesLib);
};

using ModuleHandle = typename con::ItemLib<Module>::Handle;

}  // namespace gobj

EXT_FMT(gobj::StorageType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::ModuleSlotType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::ModuleSlot,
        "(modType: {}, pos: {}, rot: {})",
        o.type,
        o.pos,
        o.rot);
EXT_FMT(gobj::ModuleType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::Module,
        "(name: {}, description: {}, type: {}, slotType: {}, textures: {})",
        o.name,
        o.description,
        o.type,
        o.slotType,
        o.textures.toString());

#endif  // LIB_MODULES_HPP