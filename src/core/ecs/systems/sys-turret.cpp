#include "sys-turret.hpp"
#ifdef SERVER
#include <engine.hpp>
#endif

namespace ecs
{

#ifdef SERVER

constexpr float kAngleErrorThreshold = 2.0f * M_PIf / 180.0f;

static inline float aimToTarget(const Transform& trSelf, const vec2& tgtPos)
{
    const vec2 turretPos = trSelf.pos;
    const vec2 dir = tgtPos - turretPos;
    const float tgtAngle = atan2f(-dir.x, dir.y) - trSelf.rot;
    return tgtAngle;
}

static inline float gotoAngle(gobj::mdata::Turret& libTurretData,
                              float& currentAngle,
                              float tgtAngle,
                              float dt)
{
    const float maxStep = libTurretData.rotSpeed * dt;
    const float delta = smath::angleError(tgtAngle, currentAngle);
    return currentAngle + std::clamp(delta, -maxStep, maxStep);
}

void sysTurretImpl(world::Sector* sector, const float dt, PtrHandle* ptrHandle)
{
    auto* reg = sector->getRegistry()->getRegistry();
    reg->view<Turret, Module, Transform, SectorId>().each(
        [ptrHandle, dt, reg, sector](auto entity,
                                     auto& turret,
                                     auto& module,
                                     auto& transform,
                                     auto& sectorId)
        {
            gobj::ModuleHandle moduleHandle = module.moduleHandle;
            gobj::Module* moduleItem =
                ptrHandle->modManager->getModuleLib().getItem(moduleHandle);
            if (moduleItem && moduleItem->type == gobj::ModuleType::Turret)
            {
                gobj::mdata::Turret libTurretData =
                    std::get<gobj::mdata::Turret>(moduleItem->data);
                // Turret rotation
                switch (turret.aimMode)
                {
                    case Turret::AimMode::Angle:
                    {
                        Turret::AngleData& angleData =
                            std::get<Turret::AngleData>(turret.aimData);
                        turret.currentAngle = angleData.angle;
                    }
                    break;
                    case Turret::AimMode::Player:
                    case Turret::AimMode::Point:
                    {
                        Turret::PointData& pointData =
                            std::get<Turret::PointData>(turret.aimData);
                        const vec2 tgtPos = pointData.pos;
                        const float tgtAngle = aimToTarget(transform, tgtPos);
                        turret.currentAngle = gotoAngle(
                            libTurretData, turret.currentAngle, tgtAngle, dt);
                    }
                    break;
                    case Turret::AimMode::Entity:
                    {
                        Turret::EntityData& entityData =
                            std::get<Turret::EntityData>(turret.aimData);
                        auto slot = ptrHandle->registryMapping->getEntity(
                            entityData.entityId);
                        if (!slot)
                        {
                            turret.aimMode = Turret::AimMode::None;
                            turret.fireMode = Turret::FireMode::None;
                            return;
                        }
                        auto* trTgt = sector->getRegistry()
                                          ->getRegistry()
                                          ->try_get<ecs::Transform>(entity);
                        if (trTgt)
                        {
                            const vec2 tgtPos = trTgt->pos;
                            const float tgtAngle =
                                aimToTarget(transform, tgtPos);
                            turret.currentAngle =
                                gotoAngle(libTurretData,
                                          turret.currentAngle,
                                          tgtAngle,
                                          dt);
                            switch (turret.fireMode)
                            {
                                case Turret::FireMode::AutoAngle:
                                    turret.isFiring =
                                        fabsf(smath::angleError(
                                            tgtAngle, turret.currentAngle))
                                        < kAngleErrorThreshold;
                                    break;
                                default:
                                    break;
                            }
                        }
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
                            std::get<Turret::ProjectileData>(turret.data);
                        if (ballisticData.reloadTimer > 0.0f)
                        {
                            ballisticData.reloadTimer -= dt;
                        }
                        else if (turret.fireMode != Turret::FireMode::None
                                 && turret.isFiring)
                        {
                            ballisticData.reloadTimer =
                                projectileData.reloadTime;
                            const float firingRot =
                                transform.rot + turret.currentAngle;
                            const float s = sinf(firingRot);
                            const float c = cosf(firingRot);
                            const vec2 exit = smath::rotateVec2(
                                libTurretData.barrelExits[0], s, c);
                            const vec2 fireDir =
                                smath::rotateVec2(vec2(0.0f, 1.0f), s, c);
                            vec2 fireVel = fireDir * projectileData.exitSpeed;
                            auto slot = ptrHandle->registryMapping->getEntity(
                                module.parent);
                            vec2 parVel = vec2(0.0f, 0.0f);
                            if (slot)
                            {
                                auto* physBody =
                                    reg->try_get<PhysicsBody>(slot->entity);
                                if (physBody)
                                {
                                    parVel = physBody->vel;
                                }
                            }
                            // PEW PEW PEW
                            sector->addSingleThreadedTask(
                                [=](ecs::PtrHandle* ptrHandle)
                                {
                                    ptrHandle->engine->spawnProjectile(
                                        sectorId.id,
                                        transform.pos + exit,
                                        fireVel,
                                        projectileData.projectile,
                                        module.parent,
                                        parVel);
                                });
                        }
                    }
                    break;
                    case def::TurretType::Railgun:
                    case def::TurretType::Missile:
                    default:
                        break;
                }
            }
        });
}
#endif
}  // namespace ecs