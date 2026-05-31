#include <algorithm>
#include <cmath>
#include <comp-struct.hpp>
#include <components/comp-ident.hpp>
#include <components/comp-phy.hpp>
#include <optional>
#include <sys-phy.hpp>

#ifdef SERVER
#include <engine.hpp>
#endif

namespace ecs
{

const float velMargin = 0.5f;
const float posDeadband = 0.1f;
const float velDeadband = 0.04f;
const float rotDeadband = 0.03f;
const float rotVelDeadband = 0.03f;

// Baumgarte stabilization: bias velocity ~ beta * max(p - slop, 0) / dt
// (clamped). Applied only on the first solver pass — contact penetration is not
// recomputed between iterations; repeating full bias + positional split each
// pass launches bodies (stale penetration × N iterations).
constexpr float kContactBaumgarte = 0.25f;
constexpr float kContactPenetrationSlop = 0.005f;
constexpr float kContactMaxBiasSpeed = 3.0f;
constexpr int kContactSolverIterations = 5;

void sysMoveCtrlImpl(world::Sector* sector,
                     entt::entity entity,
                     const ecs::EntityId& entityId,
                     float dt,
                     PtrHandle* ptrHandle)
{
    auto reg = ptrHandle->registry;
    auto* sectorId = reg->try_get<ecs::SectorId>(entity);
    auto* moveCtrl = reg->try_get<MoveCtrl>(entity);
    auto* transform = reg->try_get<Transform>(entity);
    auto* transformCache = reg->try_get<TransformCache>(entity);
    auto* physicsBody = reg->try_get<PhysicsBody>(entity);
    auto* phyThrust = reg->try_get<PhyThrust>(entity);
    if (!moveCtrl || !transform || !transformCache || !physicsBody || !phyThrust
        || !sectorId || dt <= 1e-6f)
    {
        return;
    }

    vec2 d_w;
    vec2 d_l;
    float d_l_mag;
    bool calcLocalSpaceVectorsDone = false;
    const float s = transformCache->s;
    const float c = transformCache->c;

    auto calcLocalSpaceVectors = [&](def::SectorCoords trgt)
    {
        vec2 relTargetPos = (trgt.pos.toVec2() - sectorId->toVec2())
                                * ptrHandle->world->getWorldShape().sectorSize
                            + trgt.sectorPos;
        d_w = relTargetPos - transform->pos;
        d_l = smath::rotateVec2(d_w, -s, c);
        d_l_mag = glm::length(d_l);
        calcLocalSpaceVectorsDone = true;
    };

    auto ctrlPos = [&](def::SectorCoords trgt)
    {
        // Thrust control =====================
        const float m = physicsBody->mass;
        const vec2 v_vel = physicsBody->vel;
        const vec2 v_vel_l = smath::rotateVec2(v_vel, -s, c);
        const float v = glm::length(v_vel);


        vec2 v_des_l = vec2(0.0f, 0.0f);

        moveCtrl->posReached = d_l_mag < moveCtrl->allowedPosError;
        if (!moveCtrl->posReached)
        {
            // t_ml: maximum thrust vector in object local space and
            // target direction
            const float tm_x = phyThrust->thrustManeuverMax;
            const float tm_y = phyThrust->thrustMainMax;
            const float k_t = std::min(tm_x / fabs(d_l.x), tm_y / fabs(d_l.y));
            const vec2 t_ml = k_t * d_l;
            // t_m: maximum thrust magnitude
            // a_m: maximum acceleration
            // v_m: desired velocity
            const float t_m = glm::length(t_ml);
            const float a_m = t_m / m;
            const float v_m = sqrtf(2.0f * a_m * d_l_mag);
            // v_des: desired velocity magnitude
            // v_des_l: desired velocity in object local space
            const float v_des = std::min(velMargin * v_m, phyThrust->maxSpd);
            v_des_l = v_des * d_l / d_l_mag;
        }

        const bool inPosDeadzone = d_l_mag < posDeadband && v < velDeadband;
        if (inPosDeadzone)
        {
            phyThrust->setThrustNone();
        }
        const vec2 err = v_des_l - v_vel_l;
        const vec2 thrust = ptrHandle->kpThrust * m * err;
        phyThrust->setThrustLocal(thrust, s, c);
    };

    auto ctrlVelLoc = [&](vec2 trgt)
    {
        const float m = physicsBody->mass;
        const vec2 v_vel = physicsBody->vel;
        const vec2 v_vel_l = smath::rotateVec2(v_vel, -s, c);
        const vec2 err = trgt - v_vel_l;
        const vec2 thrust = ptrHandle->kpThrust * m * err;
        phyThrust->setThrustLocal(thrust, s, c);
    };

    auto ctrlVelLocMain = [&](float trgt)
    {
        const float m = physicsBody->mass;
        const vec2 v_vel = physicsBody->vel;
        const float v_vel_l_main = smath::rotateVec2(v_vel, -s, c).y;
        const float err = trgt - v_vel_l_main;
        const float thrust = ptrHandle->kpThrust * m * err;
        phyThrust->setThrustLocalMain(thrust, s, c);
    };

    auto ctrlVelLocManeuver = [&](float trgt)
    {
        const float m = physicsBody->mass;
        const vec2 v_vel = physicsBody->vel;
        const float v_vel_l_maneuver = smath::rotateVec2(v_vel, -s, c).x;
        const float err = trgt - v_vel_l_maneuver;
        const float thrust = ptrHandle->kpThrust * m * err;
        phyThrust->setThrustLocalManeuver(thrust, s, c);
    };

    switch (moveCtrl->moveMode)
    {
        case MoveCtrl::MoveMode::MoveTo:
        {
            calcLocalSpaceVectors(moveCtrl->spPos);
            ctrlPos(moveCtrl->spPos);
            break;
        }
        case MoveCtrl::MoveMode::Brake:
        {
            ctrlVelLoc(vec2(0.0f, 0.0f));
            break;
        }
        case MoveCtrl::MoveMode::BrakeMain:
        {
            ctrlVelLocMain(0.0f);
            break;
        }
        case MoveCtrl::MoveMode::BrakeManeuver:
        {
            ctrlVelLocManeuver(0.0f);
            break;
        }
        default:
            break;
    }

    // Torque control =====================
    auto ctrlAngle = [&](float trgt)
    {
        const float angleErr = smath::angleError(trgt, transform->rot);
        moveCtrl->rotReached = std::abs(angleErr) < moveCtrl->allowedRotError;
        const float maxAngAcc = phyThrust->maxTorque / physicsBody->inertia;
        const float desWMag =
            velMargin
            * std::sqrt(std::max(0.0f, 2.0f * maxAngAcc * std::abs(angleErr)));
        const float maxRotVel = std::max(0.0f, phyThrust->maxRotVel);
        float desW = std::min(desWMag, maxRotVel);
        desW *= glm::sign(angleErr);
        const bool inRotDeadzone =
            std::abs(angleErr) < rotDeadband
            && std::abs(physicsBody->rotVel) < rotVelDeadband;
        if (inRotDeadzone)
        {
            phyThrust->setTorque(0.0f);
        }
        else
        {
            const float werr = desW - physicsBody->rotVel;
            float trq = ptrHandle->kpTurn * werr * physicsBody->inertia;
            phyThrust->setTorque(trq);
        }
    };

    auto ctrlW = [&](float trgt)
    {
        const float werr = trgt - physicsBody->rotVel;
        float trq = ptrHandle->kpTurn * werr * physicsBody->inertia;
        phyThrust->setTorque(trq);
    };

    switch (moveCtrl->turnMode)
    {
        case MoveCtrl::TurnMode::Forward:
        {
            if (!calcLocalSpaceVectorsDone)
            {
                calcLocalSpaceVectors(moveCtrl->spPos);
            }
            const float minFFDist =
                std::get<MoveCtrl::MCForwardData>(moveCtrl->faceDirData)
                    .minFaceForwardDist;
            if (d_l_mag > minFFDist)
            {
                // World is Y-down; sprites use local +Y as forward.
                // After CW rotation by `rot`, local +Y maps to
                // (-sin(rot), cos(rot)) in world — align that with
                // dir.
                moveCtrl->spRot = atan2f(-d_w.x, d_w.y);
            }
            ctrlAngle(moveCtrl->spRot);
        }
        break;
        case MoveCtrl::TurnMode::TargetPoint:
        {
            vec2 tgtDir = vec2(moveCtrl->lookAt.x - transform->pos.x,
                               moveCtrl->lookAt.y - transform->pos.y);
            if (fabs(tgtDir.x) + fabs(tgtDir.y) > ptrHandle->minFaceTargetDist)
            {
                moveCtrl->spRot = atan2f(-tgtDir.x, tgtDir.y);
            }
            ctrlW(moveCtrl->spRot);
        }
        break;
        case MoveCtrl::TurnMode::Brake:
        {
            ctrlW(0.0f);
        }
        break;
        default:
            break;
    }
}


void sysPhyThrustImpl(world::Sector* sector,
                      entt::entity entity,
                      const ecs::EntityId& entityId,
                      float dt,
                      PtrHandle* ptrHandle)
{
    auto reg = ptrHandle->registry;
    auto* physicsBody = reg->try_get<PhysicsBody>(entity);
    auto* phyThrust = reg->try_get<PhyThrust>(entity);
    if (phyThrust && physicsBody)
    {
        if (physicsBody->rotVel > phyThrust->maxRotVel && phyThrust->torque > 0)
        {
            phyThrust->torque = 0.0;
        }
        else if (physicsBody->rotVel < -phyThrust->maxRotVel
                 && phyThrust->torque < 0)
        {
            phyThrust->torque = 0.0;
        }
        physicsBody->rotAcc += phyThrust->torque / physicsBody->inertia;

        float spd = glm::length(physicsBody->vel);
        glm::vec2 thrustApply = phyThrust->thrustGlobal;
        if (!std::isfinite(thrustApply.x) || !std::isfinite(thrustApply.y))
        {
            LG_W("PhyThrust2 thrustApply is invalid");
        }
        if (spd >= phyThrust->maxSpd && spd > 1e-8f)
        {
            const glm::vec2 vhat = physicsBody->vel / spd;
            const float along = glm::dot(phyThrust->thrustGlobal, vhat);
            if (along > 0.f)
            {
                thrustApply -= vhat * along;
            }
        }
        if (!std::isfinite(thrustApply.x) || !std::isfinite(thrustApply.y))
        {
            LG_W("PhyThrust1 thrustApply is invalid");
            exit(1);
        }
        physicsBody->acc += thrustApply / physicsBody->mass;

        // Reset thrust after each update cycle
        phyThrust->torque = 0.0f;
        phyThrust->thrustGlobal = vec2(0.0f);
        phyThrust->thrustLocal = vec2(0.0f);

        // LG_D("PhyThrust update for entity: {} PhyThrust: {}", entity,
        // *phyThrust);
    }
}


void sysPhysicsImpl(world::Sector* sector,
                    entt::entity entity,
                    const ecs::EntityId& entityId,
                    float dt,
                    PtrHandle* ptrHandle)
{
    auto reg = ptrHandle->registry;
    auto* sectorId = reg->try_get<ecs::SectorId>(entity);
    auto* transform = reg->try_get<Transform>(entity);
    auto* transformCache = reg->try_get<TransformCache>(entity);
    auto* physicsBody = reg->try_get<PhysicsBody>(entity);
    auto* broadphase = reg->try_get<Broadphase>(entity);
    if (transform && physicsBody && transformCache)
    {
        physicsBody->acc += -ptrHandle->linDrag * physicsBody->vel;
        physicsBody->rotAcc += -ptrHandle->angDrag * physicsBody->rotVel;
        physicsBody->vel += physicsBody->acc * dt;
        physicsBody->rotVel += physicsBody->rotAcc * dt;
        bool hasSignificantSpd =
            (fabsf(physicsBody->vel.x) + fabsf(physicsBody->vel.y) > 1e-6f);
        bool hasSignificantRotSpd = (fabsf(physicsBody->rotVel) > 1e-5f);
        if (hasSignificantRotSpd)
        {
            transform->rot += physicsBody->rotVel * dt;
            if (transform->rot < 0.0f)
            {
                transform->rot += 2.0f * M_PIf;
            }
            else if (transform->rot >= 2.0f * M_PIf)
            {
                transform->rot -= 2.0f * M_PIf;
            }
            transformCache->c = cosf(transform->rot);
            transformCache->s = sinf(transform->rot);
        }
        if (hasSignificantSpd)
        {
            transform->pos += physicsBody->vel * dt;
        }

        auto* collider = reg->try_get<Collider>(entity);
        if (collider && (hasSignificantSpd || hasSignificantRotSpd))
        {
            const gobj::Collider* colliderDef =
                collider->getColliderDef(ptrHandle->colliderLib);
            con::AABB newAabb = calculateAABB(
                *transform, *transformCache, *collider, colliderDef);
            auto* sector = ptrHandle->world->getSector(sectorId->id);
            if (sector)
            {
                sector->moveAabbProxy(broadphase->proxyId, newAabb);
                broadphase->fatAABB = newAabb;
                sector->addBroadphaseQueryEntity(entity);
            }
        }

        // Check for sector switch
        ptrHandle->world->checkSectorSwitchAfterMove(
            entityId, entity, sectorId, transform, ptrHandle);

        // Reset acceleration after game update
        physicsBody->acc = {0, 0};
        physicsBody->rotAcc = 0;

        /*
            1.check AABB bounds and recalculate if needed
            2. if recalculated fatAABB, moveProxy in aabbTree
            3. broad phase query for object
            4. SAT for broadphase results
        */

        // LG_D("PhysicsBody update for entity: {} PhysicsBody: {}",
        // entity, *physicsBody); LG_D("Transform update for entity: {}
        // Transform:
        // {}", entity, *transform);
    }
}

struct ColResolveParams
{
    PtrHandle* ptrHandle;
    world::Sector* sector;
    const Contact& contact;
    const std::pair<entt::entity, entt::entity>& collision;
    entt::entity actionEnt;
    entt::entity otherEnt;
};

#ifdef SERVER

namespace
{

constexpr float kGoldenAngleRad = 2.39996323f;

float parentBreakupSpreadRadius(PtrHandle* ptrHandle,
                              const gobj::Asteroid& parentDef)
{
    if (!parentDef.collider.isValid())
    {
        return 1.0f;
    }
    const gobj::Collider* collider =
        ptrHandle->modManager->getColliderLib().getItem(parentDef.collider);
    if (collider == nullptr || collider->vertices.empty())
    {
        return 1.0f;
    }
    const vec2 ext = smath::colliderLocalExtents(collider->vertices);
    return std::max(0.35f, 0.45f * std::max(ext.x, ext.y));
}

void spawnParentAsteroidChildren(const ColResolveParams& p,
                                 uint32_t sectorId,
                                 const Transform& parentTransform,
                                 const gobj::AsteroidParentdata& parentData,
                                 const gobj::Asteroid& parentDef)
{
    size_t totalSpawns = 0;
    for (const gobj::AsteroidChildSpawn& entry : parentData.children)
    {
        if (entry.first.isValid() && entry.second > 0)
        {
            totalSpawns += entry.second;
        }
    }
    if (totalSpawns == 0)
    {
        return;
    }

    const float spreadRadius = parentBreakupSpreadRadius(p.ptrHandle, parentDef);
    const vec2 center = parentTransform.pos;
    size_t spawnIndex = 0;

    for (const gobj::AsteroidChildSpawn& entry : parentData.children)
    {
        if (!entry.first.isValid() || entry.second == 0)
        {
            continue;
        }
        for (uint8_t n = 0; n < entry.second; ++n)
        {
            vec2 offset = vec2(0.0f, 0.0f);
            if (totalSpawns > 1)
            {
                const float ring =
                    std::sqrt((static_cast<float>(spawnIndex) + 0.5f)
                              / static_cast<float>(totalSpawns));
                const float theta =
                    static_cast<float>(spawnIndex) * kGoldenAngleRad;
                offset = vec2(std::cos(theta), std::sin(theta)) * spreadRadius
                         * ring;
            }

            Transform childTransform = parentTransform;
            childTransform.pos = center + offset;
            if (offset.x != 0.0f || offset.y != 0.0f)
            {
                childTransform.rot =
                    std::atan2(offset.x, offset.y);
            }

            float rotVel = 0.0f;
            if (auto* parentBody =
                    p.ptrHandle->registry->try_get<PhysicsBody>(p.otherEnt))
            {
                rotVel = parentBody->rotVel * 0.5f;
            }

            p.ptrHandle->engine->spawnAsteroid(
                sectorId, childTransform, entry.first, rotVel);
            ++spawnIndex;
        }
    }
}

}  // namespace

static bool itemCollision(const ColResolveParams& p)
{
    auto reg = p.ptrHandle->registry;
    auto* item = reg->try_get<Item>(p.actionEnt);
    auto* storage = reg->try_get<Storage>(p.otherEnt);
    if (!storage || !item)
    {
        return false;
    }
    auto* itemData =
        p.ptrHandle->modManager->getItemLib().getItem(item->itemHandle);
    if (!itemData)
    {
        LG_E("Item data not found");
        return false;
    }
    uint32_t amountAdded =
        storage->tryAddItem(item->itemHandle, *itemData, item->quantity);
    item->quantity -= amountAdded;
    if (item->quantity <= 0)
    {
        ecs::EntityId actionEntId = reg->get<ecs::EntityId>(p.actionEnt);
        p.sector->markEntityForDestruction(actionEntId);
        return true;
    }
    return false;
}

static bool projectileCollision(const ColResolveParams& p)
{
    auto reg = p.ptrHandle->registry;
    auto* projectile = reg->try_get<Projectile>(p.actionEnt);
    if (!projectile)
    {
        LG_E("Projectile not found");
        return false;
    }
    auto* projectileData = p.ptrHandle->modManager->getProjectileLib().getItem(
        projectile->projectileHandle);
    if (!projectileData)
    {
        LG_E("Projectile data not found");
        return false;
    }
    auto* asteroid = reg->try_get<Asteroid>(p.otherEnt);
    if (asteroid)
    {
        float dmg = (projectileData->damageType == def::DamageType::Mining
                        ? projectileData->dmg
                        : projectileData->dmg * 0.001f) * p.ptrHandle->miningRate;

        asteroid->damage(
            p.ptrHandle,
            dmg,
            [p](gobj::ItemHandle handle, uint32_t quantity)
            {
                gobj::Item* item =
                    p.ptrHandle->modManager->getItemLib().getItem(handle);
                if (!item)
                {
                    return;
                }
                auto reg = p.ptrHandle->registry;
                auto* sectorId = reg->try_get<SectorId>(p.otherEnt);
                if (!sectorId)
                {
                    LG_E("Sector id not found");
                    return;
                }
                vec2 sourceVel = vec2(0.0f, 0.0f);
                if (auto* projBody = reg->try_get<PhysicsBody>(p.actionEnt))
                {
                    sourceVel = projBody->vel;
                }
                const Transform* astTransform =
                    reg->try_get<Transform>(p.otherEnt);
                ContactEjectParams ejectParams;
                ejectParams.computeRot = true;
                const ContactEjectSpawn eject = computeContactEjectSpawn(
                    p.contact,
                    p.otherEnt,
                    p.collision.first,
                    sourceVel,
                    astTransform != nullptr ? &astTransform->pos : nullptr,
                    ejectParams);
                Transform spawnTransform{.pos = eject.pos,
                                         .rot = eject.rot.value_or(0.0f)};
                p.ptrHandle->engine->spawnItem(
                    sectorId->id, spawnTransform, handle, quantity, eject.vel);
            });
        ecs::EntityId actionEntId = reg->get<ecs::EntityId>(p.actionEnt);
        ecs::EntityId otherEntId = reg->get<ecs::EntityId>(p.otherEnt);
        if (asteroid->volume <= 0.0f)
        {
            auto* asteroidData =
                p.ptrHandle->modManager->getAsteroidLib().getItem(
                    gobj::AsteroidHandle(asteroid->asteroidHandle));
            if (asteroidData)
            {
                if (asteroidData->type == gobj::AsteroidType::Parent)
                {
                    auto& parentData =
                        std::get<gobj::AsteroidParentdata>(asteroidData->content);
                    auto* sectorId = reg->try_get<SectorId>(p.otherEnt);
                    auto* transform = reg->try_get<Transform>(p.otherEnt);
                    if (sectorId && transform)
                    {
                        spawnParentAsteroidChildren(
                            p,
                            sectorId->id,
                            *transform,
                            parentData,
                            *asteroidData);
                    }
                }
            }
            p.sector->markEntityForDestruction(otherEntId);
        }
        p.sector->markEntityForDestruction(actionEntId);
    }
    return true;
}
#endif

static inline bool
colliderAction(PtrHandle* ptrHandle,
               world::Sector* sector,
               const Contact& contact,
               const Collider& collider1,
               const Collider& collider2,
               const std::pair<entt::entity, entt::entity>& collision)
{
    auto reg = ptrHandle->registry;
    ColResolveParams p{.ptrHandle = ptrHandle,
                       .sector = sector,
                       .contact = contact,
                       .collision = collision};

    switch (collider1.colliderType)
    {
        case CollisionLayer::Projectile:
        {
            p.actionEnt = collision.first;
            p.otherEnt = collision.second;
            return projectileCollision(p);
        }
        case CollisionLayer::Item:
        {
            p.actionEnt = collision.first;
            p.otherEnt = collision.second;
            return itemCollision(p);
        }
        default:
            break;
    }
    switch (collider2.colliderType)
    {
        case CollisionLayer::Projectile:
        {
            p.actionEnt = collision.second;
            p.otherEnt = collision.first;
            return projectileCollision(p);
        }
        case CollisionLayer::Item:
        {
            p.actionEnt = collision.second;
            p.otherEnt = collision.first;
            return itemCollision(p);
        }
        default:
            break;
    }
    return false;
}

void sysCollisionDetectionImpl(world::Sector* sector,
                               float dt,
                               PtrHandle* ptrHandle)
{
    // Query broadphase collisions from aabb tree
    auto* reg = ptrHandle->registry;
    sector->broadphaseCollisions.clear();
    sector->contactInfos.clear();

    for (auto entity : sector->broadphaseQueryEntities)
    {
        auto& broadphase = reg->get<Broadphase>(entity);
        sector->queryBroadphase(
            broadphase.fatAABB,
            [sector, entity](entt::entity entityOther)
            {
                if (entity != entityOther && entity < entityOther)
                {
                    sector->broadphaseCollisions.push_back(
                        std::make_pair(entity, entityOther));
                }
            });
    }
    for (const auto& collision : sector->broadphaseCollisions)
    {
        auto& collider1 = reg->get<Collider>(collision.first);
        auto& collider2 = reg->get<Collider>(collision.second);

        auto& Interaction = ptrHandle->collisionLayerMat->getInteraction(
            collider1.colliderType, collider2.colliderType);
        if (!Interaction.enabled)
        {
            continue;
        }

        const ecs::EntityId* exceptEntity1 =
            (ecs::EntityId*)&collider1.exceptEntity;
        if (*exceptEntity1 != ecs::EntityId::Invalid())
        {
            entt::entity except = ptrHandle->ecs->getEntity(*exceptEntity1);
            if (except == collision.second)
            {
                continue;
            }
        }
        const ecs::EntityId* exceptEntity2 =
            (ecs::EntityId*)&collider2.exceptEntity;
        if (*exceptEntity2 != ecs::EntityId::Invalid())
        {
            entt::entity except = ptrHandle->ecs->getEntity(*exceptEntity2);
            if (except == collision.first)
            {
                continue;
            }
        }

        auto& transform1 = reg->get<Transform>(collision.first);
        auto& transform2 = reg->get<Transform>(collision.second);
        auto& transformCache1 = reg->get<TransformCache>(collision.first);
        auto& transformCache2 = reg->get<TransformCache>(collision.second);
        const gobj::Collider* colliderDef1 =
            collider1.getColliderDef(ptrHandle->colliderLib);
        const gobj::Collider* colliderDef2 =
            collider2.getColliderDef(ptrHandle->colliderLib);

        const std::optional<Contact> contact =
            ::ecs::collideCollidersWorld(collider1,
                                         colliderDef1,
                                         transform1,
                                         transformCache1,
                                         collider2,
                                         colliderDef2,
                                         transform2,
                                         transformCache2);
        if (contact)
        {
            bool skipContactSolver = colliderAction(
                ptrHandle, sector, *contact, collider1, collider2, collision);
            if (!skipContactSolver)
            {
                sector->contactInfos.push_back(
                    {*contact,
                     collision.first,
                     collision.second,
                     std::fmin(collider1.getRestitution(colliderDef1),
                               collider2.getRestitution(colliderDef2))});
            }
        }
    }
    for (int i = 0; i < kContactSolverIterations; ++i)
    {
        for (const auto& contactInfo : sector->contactInfos)
        {
            auto& contact = contactInfo.contact;
            auto* phy1 = reg->try_get<PhysicsBody>(contactInfo.ent1);
            auto* phy2 = reg->try_get<PhysicsBody>(contactInfo.ent2);
            auto& transform1 = reg->get<Transform>(contactInfo.ent1);
            auto& transform2 = reg->get<Transform>(contactInfo.ent2);

            // One-shot projection: penetration in contact is from
            // the pre-solve narrowphase pass only; do not re-apply
            // per iter.
            if (i == 0)
            {
                const vec2 correction =
                    contact.normal * contact.penetration * 0.5f;
                if (phy1)
                {
                    transform1.pos -= correction;
                }
                if (phy2)
                {
                    transform2.pos += correction;
                }
            }
            vec2 vel1 = vec2(0.0f, 0.0f);
            float mass1 = 100000.0f;
            if (phy1)
            {
                vel1 = phy1->vel;
                mass1 = phy1->mass;
            }
            vec2 vel2 = vec2(0.0f, 0.0f);
            float mass2 = 100000.0f;
            if (phy2)
            {
                vel2 = phy2->vel;
                mass2 = phy2->mass;
            }

            const vec2 rv = vel2 - vel1;
            const float velAlongNormal = glm::dot(rv, contact.normal);

            const float invMass1 = 1.0f / mass1;
            const float invMass2 = 1.0f / mass2;
            const float denom = invMass1 + invMass2;
            if (denom < 1e-12f)
            {
                continue;
            }

            float penBias = 0.0f;
            if (i == 0 && contact.penetration > kContactPenetrationSlop)
            {
                const float invDt = dt > 1e-8f ? 1.0f / dt : 0.0f;
                penBias = kContactBaumgarte * invDt
                          * (contact.penetration - kContactPenetrationSlop);
                penBias = std::min(penBias, kContactMaxBiasSpeed);
            }

            const float e = contactInfo.restitution;
            float jN = 0.0f;
            if (velAlongNormal < 0.0f)
            {
                jN = -(1.0f + e) * velAlongNormal;
            }
            const float j = (jN + penBias) / denom;
            const vec2 impulse = contact.normal * j;
            if (phy1)
            {
                phy1->vel -= impulse * invMass1;
            }
            if (phy2)
            {
                phy2->vel += impulse * invMass2;
            }
        }
    }
}


void sysAnchorFixedImpl(world::Sector* sector,
                        entt::entity entity,
                        const ecs::EntityId& entityId,
                        float dt,
                        PtrHandle* ptrHandle)
{
    auto reg = ptrHandle->registry;
    auto* anchorFixed = reg->try_get<AnchorFixed>(entity);
    auto* transform = reg->try_get<Transform>(entity);
    auto* sectorId = reg->try_get<ecs::SectorId>(entity);
    if (anchorFixed && transform)
    {
        entt::entity parent = ptrHandle->ecs->getEntity(anchorFixed->ref);
        if (parent != entt::null)
        {
            const auto& parentTransform = reg->get<Transform>(parent);
            const auto& parentTransformCache = reg->get<TransformCache>(parent);
            const auto& parentSectorId = reg->get<ecs::SectorId>(parent);
            vec2 anchorFixedPos = smath::rotateVec2(anchorFixed->pos,
                                                    parentTransformCache.s,
                                                    parentTransformCache.c);
            transform->pos = parentTransform.pos + anchorFixedPos;
            transform->rot = parentTransform.rot + anchorFixed->rot;
            if (parentSectorId.id != sectorId->id)
            {
                ptrHandle->world->addSectorMoveRequest(
                    ptrHandle, entityId, parentSectorId.id);
            }
        }
    }
}

}  // namespace ecs