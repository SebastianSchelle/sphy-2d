#ifndef LIB_MODULES_HPP
#define LIB_MODULES_HPP

#include <lib-textures.hpp>
#include <magic_enum/magic_enum.hpp>
#include <ship-def.hpp>
#include <std-inc.hpp>
#include <turret-def.hpp>
#include <yaml-cpp/yaml.h>
#include <lib-projectile.hpp>
#include <lib-item.hpp>

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
    NumSlotTypes
};

constexpr int8_t
    ModuleSlotZOffset[static_cast<size_t>(ModuleSlotType::NumSlotTypes)] = {
        -5,  // ThrusterMainS_Common
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

inline bool moduleSlotTypeHasAngleLimits(ModuleSlotType type)
{
    switch (type)
    {
        case ModuleSlotType::RoofS_Common:
        case ModuleSlotType::RoofM_Common:
        case ModuleSlotType::RoofL_Common:
        case ModuleSlotType::BayS_Common:
        case ModuleSlotType::BayM_Common:
        case ModuleSlotType::BayL_Common:
            return true;
        default:
            return false;
    }
}

struct ModuleSlot
{
    ModuleSlotType type;
    vec2 pos;
    float rot;
    /** Allowed module yaw relative to slot forward (radians); roof/bay slots only. */
    float minAngle = 0.0f;
    float maxAngle = 0.0f;
};

enum class ModuleType : uint8_t
{
    None,
    MainThruster,
    ManeuverThruster,
    Storage,
    Hangar,
    Turret,
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
    float volume[static_cast<size_t>(gobj::ItemStorageType::NumStorageTypes)];
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
    struct ProjectileData
    {
        float projDmg = 1.0f;
        float exitSpeed = 1000.0f;
        float reloadTime = 1.0f;
        ProjectileHandle projectile = ProjectileHandle::Invalid();
    };
    struct LaserData
    {
        float dps = 1.0f;
        float beamWidth = 1.0f;
        float beamLength = 1000.0f;
        uint32_t beamColor = 0xFFFFFFFF;
    };
    struct ArcData
    {
        float dps = 1.0f;
        float arcAngle = 10.0f;
        float arcLength = 1000.0f;
        uint32_t arcColor = 0xFFFFFFFF;
    };
    struct MissileData
    {
        MissileHandle missile = MissileHandle::Invalid();
    };
    struct RailgunData
    {
        float projDmg = 1.0f;
        float exitSpeed = 1000.0f;
        ProjectileHandle projectile = ProjectileHandle::Invalid();
    };
    typedef std::variant<ProjectileData,
                         LaserData,
                         ArcData,
                         MissileData,
                         RailgunData>
        TurretData;
    TurretData data = ProjectileData{};
    def::TurretType type = def::TurretType::Projectile;
    uint8_t numBarrels = 1;
    vector<vec2> barrelExits;
    float rotSpeed = 1.0f;
    float range = 1.0f;
    static Turret fromYaml(
        const YAML::Node& node,
        con::ItemLib<gobj::Projectile>& projectileLib,
        const con::ItemLib<gobj::Missile>& missileLib);
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
    TexturesHandle texturesBase = TexturesHandle::Invalid();
    ModuleType type;
    float mass;
    mdata::Data data;

    static Module fromYaml(
        const YAML::Node& node,
        const con::ItemLib<gobj::Textures>& texturesLib,
        con::ItemLib<gobj::Projectile>& projectileLib,
        const con::ItemLib<gobj::Missile>& missileLib);
};

using ModuleHandle = typename con::ItemLib<Module>::Handle;

}  // namespace gobj

EXT_FMT(gobj::ModuleSlotType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::ModuleSlot,
        "(modType: {}, pos: {}, rot: {}, minAngle: {}, maxAngle: {})",
        o.type,
        o.pos,
        o.rot,
        o.minAngle,
        o.maxAngle);
EXT_FMT(gobj::ModuleType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::Module,
        "(name: {}, description: {}, type: {}, slotType: {}, textures: {})",
        o.name,
        o.description,
        o.type,
        o.slotType,
        o.textures.toString());

#endif  // LIB_MODULES_HPP
