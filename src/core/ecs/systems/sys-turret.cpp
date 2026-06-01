#include "sys-turret.hpp"
#ifdef SERVER
#include <engine.hpp>
#endif

namespace ecs
{

void sysTurretImpl(world::Sector* sector,
                   const entt::entity entity,
                   const ecs::EntityId& entityId,
                   const float dt,
                   PtrHandle* ptrHandle)
{
    auto reg = ptrHandle->registry;
    auto* turret = reg->try_get<Turret>(entity);
    auto* module = reg->try_get<Module>(entity);
    auto* transform = reg->try_get<Transform>(entity);
    auto* sectorId = reg->try_get<SectorId>(entity);
    if (turret && module && transform && sectorId)
    {
        gobj::ModuleHandle moduleHandle = module->moduleHandle;
        gobj::Module* moduleItem =
            ptrHandle->modManager->getModuleLib().getItem(moduleHandle);
        if (moduleItem && moduleItem->type == gobj::ModuleType::Turret)
        {
            gobj::mdata::Turret libTurretData =
                std::get<gobj::mdata::Turret>(moduleItem->data);

            // Turret rotation
            switch (turret->aimMode)
            {
                case Turret::AimMode::Angle:
                {
                    Turret::AngleData& angleData =
                        std::get<Turret::AngleData>(turret->aimData);
                    turret->currentAngle = angleData.angle;
                }
                break;
                case Turret::AimMode::Point:
                {
                    Turret::PointData& pointData =
                        std::get<Turret::PointData>(turret->aimData);
                    const vec2 turretPos = transform->pos;
                    const vec2 targetPos = pointData.pos;
                    const vec2 dir = targetPos - turretPos;
                    const float tgtAngle =
                        atan2f(-dir.x, dir.y) - transform->rot;
                    const float maxStep = libTurretData.rotSpeed * dt;
                    const float delta =
                        smath::angleError(tgtAngle, turret->currentAngle);
                    turret->currentAngle +=
                        std::clamp(delta, -maxStep, maxStep);
                }
                break;
                case Turret::AimMode::Entity:
                {
                    // Turret::EntityData& entityData =
                    // std::get<Turret::EntityData>(turret->aimData);
                    // turret->currentAngle = entityData.entityId;
                }
                break;
                default:
                    break;
            }

            // Turret firing
            switch (libTurretData.type)
            {
                case def::TurretType::Projectile:
                {
                    gobj::mdata::Turret::ProjectileData& projectileData =
                        std::get<gobj::mdata::Turret::ProjectileData>(
                            libTurretData.data);
                    Turret::ProjectileData& ballisticData =
                        std::get<Turret::ProjectileData>(turret->data);
                    if (ballisticData.reloadTimer > 0.0f)
                    {
                        ballisticData.reloadTimer -= dt;
                    }
                    else if (turret->isFiring)
                    {
                        ballisticData.reloadTimer = projectileData.reloadTime;
#ifdef SERVER
                        const float firingRot =
                            transform->rot + turret->currentAngle;
                        const float s = sinf(firingRot);
                        const float c = cosf(firingRot);
                        const vec2 exit = smath::rotateVec2(
                            libTurretData.barrelExits[0], s, c);
                        const vec2 fireDir =
                            smath::rotateVec2(vec2(0.0f, 1.0f), s, c);
                        vec2 fireVel = fireDir * projectileData.exitSpeed;
                        entt::entity parentEntt =
                            ptrHandle->ecs->getEntity(module->parent);
                        vec2 parVel = vec2(0.0f, 0.0f);
                        if (parentEntt != entt::null)
                        {
                            auto* physBody =
                                reg->try_get<PhysicsBody>(parentEntt);
                            if (physBody)
                            {
                                parVel = physBody->vel;
                            }
                        }
                        ptrHandle->engine->spawnProjectile(
                            sectorId->id,
                            transform->pos + exit,
                            fireVel,
                            projectileData.projectile,
                            module->parent,
                            parVel);
#endif
                    }
                }
                break;
                case def::TurretType::Railgun:
                case def::TurretType::Missile:
                default:
                    break;
            }
        }
    }
}

}  // namespace ecs