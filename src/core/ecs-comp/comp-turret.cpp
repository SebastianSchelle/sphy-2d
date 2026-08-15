#include "comp-turret.hpp"

namespace ecs
{

Turret::TurretData Turret::fromGobjTurretData(const gobj::mdata::Turret& turretData)
{
    switch (turretData.type)
    {
        case def::TurretType::Projectile:
            return ProjectileData{0.0f};
        case def::TurretType::Laser:
            return LaserData{};
        case def::TurretType::Arc:
            return ArcData{};
        case def::TurretType::Missile:
            return MissileData{};
        case def::TurretType::Railgun:
            return RailgunData{};
        default:
            return ProjectileData{0.0f};
    }
}

void Turret::setAimMode(AimMode aimMode)
{
    this->aimMode = aimMode;
    switch (this->aimMode)
    {
        case AimMode::Angle:
            aimData = AngleData{0.0f};
            break;
        case AimMode::Point:
            aimData = PointData{vec2(0.0f, 0.0f)};
            break;
        case AimMode::Entity:
            aimData = EntityData{EntityId::Invalid()};
            break;
        case AimMode::Player:
            aimData = PointData{vec2(0.0f, 0.0f)};
            break;
        case AimMode::None:
            aimData = AngleData{0.0f};
            break;
        case AimMode::NumAimModes:
            break;
    }
}

}  // namespace ecs