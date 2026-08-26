#include "sys-turret.hpp"
#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "comp-turret.hpp"
#include "entt/entity/fwd.hpp"
#include "lib-projectile.hpp"
#include "logging.hpp"
#include "ptr-handle.hpp"
#include "turret-def.hpp"
#include <engine.hpp>

namespace ecs
{

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

inline static void calcExit(const ecs::Transform& parentTr,
                            const ecs::Turret& turr,
                            const vec2& barrelExit,
                            vec2& exit,
                            float& rot)
{
    rot = parentTr.rot + turr.currentAngle;
    const float s = sinf(rot);
    const float c = cosf(rot);
    exit = parentTr.pos + smath::rotateVec2(barrelExit, s, c);
}


inline static void calcExitNDir(const ecs::Transform& parentTr,
                                const ecs::Turret& turr,
                                float exitSpeed,
                                const vec2& barrelExit,
                                vec2& exit,
                                float& rot,
                                vec2& vel)
{
    rot = parentTr.rot + turr.currentAngle;
    const float s = sinf(rot);
    const float c = cosf(rot);
    exit = parentTr.pos + smath::rotateVec2(barrelExit, s, c);
    const vec2 fireDir = smath::rotateVec2(vec2(0.0f, 1.0f), s, c);
    vel = fireDir * exitSpeed;
}

inline vec2 getParentVel(ecs::PtrHandle* ptrHandle,
                         entt::registry* reg,
                         ecs::EntityId parent)
{
    vec2 parVel = vec2(0.0f, 0.0f);
    auto slot = ptrHandle->registryMapping->getEntity(parent);
    if (slot)
    {
        auto* physBody = reg->try_get<PhysicsBody>(slot->entity);
        if (physBody)
        {
            parVel = physBody->vel;
        }
    }
    return parVel;
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
                        auto* trTgt =
                            sector->getRegistry()
                                ->getRegistry()
                                ->try_get<ecs::Transform>(slot->entity);
                        if (trTgt)
                        {
                            const vec2 tgtPos = trTgt->pos;
                            const float tgtAngle =
                                aimToTarget(transform, tgtPos);
                            turret.currentAngle = gotoAngle(libTurretData,
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
                            const gobj::Projectile* proj =
                                ptrHandle->modManager->getProjectileLib()
                                    .getItem(projectileData.projectile);
                            if (!proj)
                            {
                                break;
                            }
                            ballisticData.reloadTimer =
                                projectileData.reloadTime;

                            vec2 exit;
                            float rot;
                            vec2 vel;
                            calcExitNDir(transform,
                                         turret,
                                         projectileData.exitSpeed,
                                         libTurretData.barrelExits[0],
                                         exit,
                                         rot,
                                         vel);
                            vec2 parVel =
                                getParentVel(ptrHandle, reg, module.parent);
                            sector->spawnProjectile(opool::Projectile{
                                .transform = ecs::Transform{exit, rot},
                                .collExcept = module.parent,
                                .proj = projectileData.projectile,
                                .vel = parVel + vel,
                                .lifetimeMax = proj->lifetime});
                        }
                    }
                    break;
                    case def::TurretType::Railgun:
                    case def::TurretType::Missile:
                        break;
                    case def::TurretType::Laser:
                    {
                        using LState = Turret::LaserData::LaserState;
                        gobj::mdata::Turret::LaserData& laserData =
                            std::get<gobj::mdata::Turret::LaserData>(
                                libTurretData.data);
                        Turret::LaserData& laserState =
                            std::get<Turret::LaserData>(turret.data);

                        const bool fire =
                            turret.fireMode != Turret::FireMode::None
                            && turret.isFiring;
                        if (laserState.state == LState::Off)
                        {
                            if ((laserState.timer >= 0.0f && laserData.offTime > 0.0001f))
                            {
                                laserState.timer -= dt;
                            }
                            else if (fire)
                            {
                                vec2 exit;
                                float rot;
                                calcExit(transform,
                                         turret,
                                         libTurretData.barrelExits[0],
                                         exit,
                                         rot);
                                laserState.beam = sector->spawnBeam(opool::Beam{
                                    .origin{.pos = exit, .rot = rot},
                                    .collExcept = module.parent,
                                    .beam = laserData.beam});
                                laserState.timer = laserData.onTime;
                                laserState.state = LState::On;
                                LG_D("Laser on");
                            }
                        }
                        else
                        {
                            if ((laserState.timer >= 0.0f || laserData.offTime <= 0.0001f) && fire)
                            {
                                laserState.timer -= dt;
                                auto* beam = sector->getBeam(laserState.beam);
                                if (beam)
                                {
                                    vec2 exit;
                                    float rot;
                                    calcExit(transform,
                                             turret,
                                             libTurretData.barrelExits[0],
                                             exit,
                                             rot);
                                    beam->origin.pos = exit;
                                    beam->origin.rot = rot;
                                }
                            }
                            else
                            {
                                sector->removeBeam(laserState.beam);
                                laserState.timer = laserData.offTime;
                                laserState.state = LState::Off;
                            }
                        }
                    }
                    break;
                    default:
                        break;
                }
            }
        });
}

}  // namespace ecs