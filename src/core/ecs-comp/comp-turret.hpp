#ifndef COMP_TURRET_HPP
#define COMP_TURRET_HPP

#include "comp-ident.hpp"
#include <lib-modules.hpp>
#include <std-inc.hpp>
#include <turret-def.hpp>

namespace ecs
{

struct Turret
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "turret";

    enum class AimMode : uint8_t
    {
        None,
        Angle,
        Point,
        Entity,
        Player,
        NumAimModes,
    };
    enum class FireMode : uint8_t
    {
        None,
        Manual,
        AutoAngle,
        NumFireModes,
    };

    struct AngleData
    {
        float angle = 0.0f;
    };
    struct PointData
    {
        vec2 pos = vec2(0.0f, 0.0f);
    };
    struct EntityData
    {
        ecs::EntityId entityId;
    };
    typedef std::variant<AngleData, PointData, EntityData> AimData;

    struct ProjectileData
    {
        float reloadTimer = 0.0f;
    };
    struct LaserData
    {
    };
    struct PlasmaData
    {
    };
    struct ArcData
    {
    };
    struct MissileData
    {
    };
    struct RailgunData
    {
    };
    struct MiningData
    {
    };
    typedef std::variant<ProjectileData,
                         LaserData,
                         PlasmaData,
                         ArcData,
                         MissileData,
                         RailgunData,
                         MiningData>
        TurretData;

    static TurretData fromGobjTurretData(const gobj::mdata::Turret& turretData);
    void setAimMode(AimMode aimMode);

    AimMode aimMode = AimMode::None;
    FireMode fireMode = FireMode::None;
    def::TurretType type = def::TurretType::Projectile;
    AimData aimData = AngleData{0.0f};
    TurretData data = ProjectileData{};
    float currentAngle = 0.0f;
    bool isFiring = false;
};


#define SER_TURRET_ANGLE_DATA S4b(o.angle);
EXT_SER(Turret::AngleData, SER_TURRET_ANGLE_DATA)
EXT_DES(Turret::AngleData, SER_TURRET_ANGLE_DATA)

#define SER_TURRET_POINT_DATA SOBJ(o.pos);
EXT_SER(Turret::PointData, SER_TURRET_POINT_DATA)
EXT_DES(Turret::PointData, SER_TURRET_POINT_DATA)

#define SER_TURRET_ENTITY_DATA SOBJ(o.entityId);
EXT_SER(Turret::EntityData, SER_TURRET_ENTITY_DATA)
EXT_DES(Turret::EntityData, SER_TURRET_ENTITY_DATA)

#define SER_TURRET                                                             \
    {                                                                          \
        uint8_t aimModeRaw = static_cast<uint8_t>(o.aimMode);                  \
        s.value1b(aimModeRaw);                                                 \
        o.aimMode = static_cast<Turret::AimMode>(aimModeRaw);                  \
    }                                                                          \
    {                                                                          \
        uint8_t fireModeRaw = static_cast<uint8_t>(o.fireMode);                \
        s.value1b(fireModeRaw);                                                \
        o.fireMode = static_cast<Turret::FireMode>(fireModeRaw);               \
    }                                                                          \
    S4b(o.currentAngle);                                                       \
    switch (o.aimMode)                                                         \
    {                                                                          \
        case Turret::AimMode::None:                                            \
        {                                                                      \
            break;                                                             \
        }                                                                      \
        case Turret::AimMode::Angle:                                           \
        {                                                                      \
            Turret::AngleData data =                                           \
                std::holds_alternative<Turret::AngleData>(o.aimData)           \
                    ? std::get<Turret::AngleData>(o.aimData)                   \
                    : Turret::AngleData{0.0f};                                 \
            s.object(data);                                                    \
            o.aimData = data;                                                  \
            break;                                                             \
        }                                                                      \
        case Turret::AimMode::Point:                                           \
        {                                                                      \
            Turret::PointData data =                                           \
                std::holds_alternative<Turret::PointData>(o.aimData)           \
                    ? std::get<Turret::PointData>(o.aimData)                   \
                    : Turret::PointData{vec2(0.0f, 0.0f)};                     \
            s.object(data);                                                    \
            o.aimData = data;                                                  \
            break;                                                             \
        }                                                                      \
        case Turret::AimMode::Entity:                                          \
        {                                                                      \
            Turret::EntityData data =                                          \
                std::holds_alternative<Turret::EntityData>(o.aimData)          \
                    ? std::get<Turret::EntityData>(o.aimData)                  \
                    : Turret::EntityData{ecs::EntityId::Invalid()};            \
            s.object(data);                                                    \
            o.aimData = data;                                                  \
            break;                                                             \
        }                                                                      \
        case Turret::AimMode::Player:                                          \
        {                                                                      \
            Turret::PointData data =                                           \
                std::holds_alternative<Turret::PointData>(o.aimData)           \
                    ? std::get<Turret::PointData>(o.aimData)                   \
                    : Turret::PointData{vec2(0.0f, 0.0f)};                     \
            s.object(data);                                                    \
            o.aimData = data;                                                  \
            break;                                                             \
        }                                                                      \
        default:                                                               \
            o.aimMode = Turret::AimMode::Angle;                                \
            o.aimData = Turret::AngleData{0.0f};                               \
            break;                                                             \
    }                                                                          \
    switch (o.type)                                                            \
    {                                                                          \
        default:                                                               \
            break;                                                             \
    }
EXT_SER(Turret, SER_TURRET)
EXT_DES(Turret, SER_TURRET)

}  // namespace ecs

#endif  // COMP_TURRET_HPP