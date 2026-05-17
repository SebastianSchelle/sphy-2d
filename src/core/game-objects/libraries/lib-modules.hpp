#ifndef LIB_MODULES_HPP
#define LIB_MODULES_HPP

#include <lib-textures.hpp>
#include <magic_enum/magic_enum.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

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


/* Order must match ModuleSlotType (excluding NumSlotTypes). S/M/L -> 2/5/10. */
constexpr float SlotSize[static_cast<size_t>(ModuleSlotType::NumSlotTypes)] = {
    5.0f,   // ThrusterMainS_Common
    10.0f,  // ThrusterMainM_Common
    20.0f,  // ThrusterMainL_Common
    5.0f,   // ThrusterManeuverS_Common
    10.0f,  // ThrusterManeuverM_Common
    20.0f,  // ThrusterManeuverL_Common
    2.0f,   // InternalS_Common
    5.0f,   // InternalM_Common
    10.0f,  // InternalL_Common
    2.0f,   // RoofS_Common
    5.0f,   // RoofM_Common
    10.0f,  // RoofL_Common
    20.0f,  // BayS_Common
    5.0f,   // BayM_Common
    10.0f,  // BayL_Common
};

constexpr int8_t
    ModuleSlotZOffset[static_cast<size_t>(ModuleSlotType::NumSlotTypes)] = {
        10,   // ThrusterMainS_Common
        10,   // ThrusterMainM_Common
        10,   // ThrusterMainL_Common
        10,   // ThrusterManeuverS_Common
        10,   // ThrusterManeuverM_Common
        10,   // ThrusterManeuverL_Common
        10,   // InternalS_Common
        10,   // InternalM_Common
        10,   // InternalL_Common
        -10,  // RoofS_Common
        -10,  // RoofM_Common
        -10,  // RoofL_Common
        -10,  // BayS_Common
        -10,  // BayM_Common
        -10,  // BayL_Common
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
    static Hangar fromYaml(const YAML::Node& node);
};

using Data = std::variant<MainThruster, ManeuverThruster, Storage, Hangar>;

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