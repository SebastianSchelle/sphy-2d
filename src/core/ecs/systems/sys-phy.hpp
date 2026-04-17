#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include <algorithm>
#include <cmath>
#include <components/comp-ident.hpp>
#include <components/comp-phy.hpp>
#include <ecs.hpp>
#include <optional>
#include <std-inc.hpp>
#include <world.hpp>

namespace ecs
{

const float velMargin = 0.8f;
const float posDeadband = 0.1f;
const float velDeadband = 0.04f;
const float rotDeadband = 0.03f;
const float rotVelDeadband = 0.03f;

// Baumgarte stabilization: bias velocity ~ beta * max(p - slop, 0) / dt (clamped).
// Applied only on the first solver pass — contact penetration is not recomputed
// between iterations; repeating full bias + positional split each pass launches
// bodies (stale penetration × N iterations).
constexpr float kContactBaumgarte = 0.25f;
constexpr float kContactPenetrationSlop = 0.005f;
constexpr float kContactMaxBiasSpeed = 3.0f;
constexpr int kContactSolverIterations = 5;

const System sysMoveCtrl = {
    .name = "sysMoveCtrl",
    .type = SystemType::SectorForeachEntitiy,
    .function = SFSectorForeach{
        [](world::Sector* sector,
           entt::entity entity,
           const ecs::EntityId& entityId,
           float dt,
           PtrHandle* ptrHandle)
        {
            auto reg = ptrHandle->registry;
            auto* sectorId = reg->try_get<ecs::SectorId>(entity);
            auto* moveCtrl = reg->try_get<MoveCtrl>(entity);
            auto* transform = reg->try_get<Transform>(entity);
            auto* physicsBody = reg->try_get<PhysicsBody>(entity);
            auto* phyThrust = reg->try_get<PhyThrust>(entity);
            if (!moveCtrl || !transform || !physicsBody || !phyThrust
                || !sectorId || !moveCtrl->active || dt <= 1e-6f)
            {
                return;
            }

            // Thrust control =====================
            vec2 relTargetPos =
                (vec2(moveCtrl->spPos.pos.x, moveCtrl->spPos.pos.y)
                 - vec2(sectorId->x, sectorId->y))
                    * ptrHandle->world->getWorldShape().sectorSize
                + moveCtrl->spPos.sectorPos;
            const vec2 worldDir = relTargetPos - transform->pos;
            const float dist = length(worldDir);
            const float speed = length(physicsBody->vel);
            const bool inPosDeadzone =
                dist < posDeadband && speed < velDeadband;
            vec2 dir;
            if (inPosDeadzone)
            {
                phyThrust->setThrustGlobal(vec2(0.0f), *transform);
            }
            else
            {
                glm::vec2 desVelV(0.0f);
                const float maxAcc =
                    fmaxf(1e-6f,
                          std::min(phyThrust->thrustManeuverMax,
                                   phyThrust->thrustMainMax)
                              / physicsBody->mass);
                dir = worldDir / dist;
                const float desVelMag = velMargin * sqrtf(2.0f * maxAcc * dist);
                const float desVel = fminf(phyThrust->maxSpd, desVelMag);
                desVelV = dir * desVel;

                const glm::vec2 err = desVelV - physicsBody->vel;
                const glm::vec2 thrust =
                    ptrHandle->kpThrust * physicsBody->mass * err;
                phyThrust->setThrustGlobal(thrust, *transform);
            }

            // Torque control =====================

            switch (moveCtrl->faceDirMode)
            {
                case MoveCtrl::FaceDirMode::Forward:
                    if (dist > ptrHandle->minFaceForwardDist)
                    {
                        moveCtrl->spRot = atan2f(dir.y, dir.x);
                    }
                    break;
                case MoveCtrl::FaceDirMode::TargetPoint:
                {
                    vec2 tgtDir = vec2(moveCtrl->lookAt.x - transform->pos.x,
                                       moveCtrl->lookAt.y - transform->pos.y);
                    if (fabs(tgtDir.x) + fabs(tgtDir.y)
                        > ptrHandle->minFaceTargetDist)
                    {
                        moveCtrl->spRot = atan2f(tgtDir.y, tgtDir.x);
                    }
                }
                break;
                default:
                    break;
            }

            const float angleErr =
                smath::angleError(moveCtrl->spRot, transform->rot);
            const bool inRotDeadzone =
                std::abs(angleErr) < rotDeadband
                && std::abs(physicsBody->rotVel) < rotVelDeadband;
            float desW = 0.0f;
            if (inRotDeadzone)
            {
                phyThrust->setTorque(0.0f);
            }
            else
            {
                const float maxAngAcc =
                    phyThrust->maxTorque / physicsBody->inertia;
                const float desWMag =
                    velMargin
                    * std::sqrt(
                        std::max(0.0f, 2.0f * maxAngAcc * std::abs(angleErr)));
                const float maxRotVel = std::max(0.0f, phyThrust->maxRotVel);
                desW = std::min(desWMag, maxRotVel);
                desW *= glm::sign(angleErr);
                const float werr = desW - physicsBody->rotVel;
                float trq = ptrHandle->kpTurn * werr * physicsBody->inertia;
                phyThrust->setTorque(trq);
            }
        }}};

const System sysPhyThrust = {
    .name = "sysPhyThrust",
    .type = SystemType::SectorForeachEntitiy,
    .function = SFSectorForeach{
        [](world::Sector* sector,
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
                if (physicsBody->rotVel > phyThrust->maxRotVel
                    && phyThrust->torque > 0)
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
                if (spd >= phyThrust->maxSpd && spd > 1e-8f)
                {
                    const glm::vec2 vhat = physicsBody->vel / spd;
                    const float along = glm::dot(phyThrust->thrustGlobal, vhat);
                    if (along > 0.f)
                    {
                        thrustApply -= vhat * along;
                    }
                }
                physicsBody->acc += thrustApply / physicsBody->mass;

                // Reset thrust after each update cycle
                phyThrust->torque = 0.0f;
                phyThrust->thrustGlobal = vec2(0.0f);
                phyThrust->thrustLocal = vec2(0.0f);

                // LG_D("PhyThrust update for entity: {} PhyThrust: {}", entity,
                // *phyThrust);
            }
        }}};

const System sysPhysics = {
    .name = "sysPhysics",
    .type = SystemType::SectorForeachEntitiy,
    .function = SFSectorForeach{
        [](world::Sector* sector,
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
            auto* collider = reg->try_get<Collider>(entity);
            auto* broadphase = reg->try_get<Broadphase>(entity);
            if (transform && physicsBody && transformCache && collider)
            {
                physicsBody->acc += -ptrHandle->linDrag * physicsBody->vel;
                physicsBody->rotAcc +=
                    -ptrHandle->angDrag * physicsBody->rotVel;
                physicsBody->vel += physicsBody->acc * dt;
                physicsBody->rotVel += physicsBody->rotAcc * dt;
                bool hasSignificantSpd =
                    (fabsf(physicsBody->vel.x) + fabsf(physicsBody->vel.y)
                     > 1e-6f);
                bool hasSignificantRotSpd =
                    (fabsf(physicsBody->rotVel) > 1e-5f);
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
                if (hasSignificantSpd || hasSignificantRotSpd)
                {
                    con::AABB newAabb =
                        calculateAABB(*transform, *transformCache, *collider);
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
        }}};

const System sysCollisionDetection = {
    .name = "sysCollisionDetection",
    .type = SystemType::SectorOnce,
    .function =
        SFSectorOnce{
            [](world::Sector* sector,
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
                            if (entity != entityOther)
                            {
                                sector->broadphaseCollisions.push_back(
                                    std::make_pair(entity, entityOther));
                            }
                        });
                }
                // Erase duplicates from the broadphase collisions
                std::sort(sector->broadphaseCollisions.begin(),
                          sector->broadphaseCollisions.end(),
                          [](const std::pair<entt::entity, entt::entity>& a,
                             const std::pair<entt::entity, entt::entity>& b)
                          { return a.first < b.first; });
                sector->broadphaseCollisions.erase(
                    std::unique(sector->broadphaseCollisions.begin(),
                                sector->broadphaseCollisions.end()),
                    sector->broadphaseCollisions.end());
                for (const auto& collision : sector->broadphaseCollisions)
                {
                    // LG_D(
                    //     "Broadphase collision detected between entities: {} "
                    //     "and {}",
                    //     collision.first,
                    //     collision.second);
                    auto& transform1 = reg->get<Transform>(collision.first);
                    auto& transform2 = reg->get<Transform>(collision.second);
                    auto& transformCache1 =
                        reg->get<TransformCache>(collision.first);
                    auto& transformCache2 =
                        reg->get<TransformCache>(collision.second);
                    auto& collider1 = reg->get<Collider>(collision.first);
                    auto& collider2 = reg->get<Collider>(collision.second);

                    const std::optional<Contact> contact =
                        ::ecs::collideCollidersWorld(collider1,
                                                     transform1,
                                                     transformCache1,
                                                     collider2,
                                                     transform2,
                                                     transformCache2);
                    if (contact)
                    {
                        sector->contactInfos.push_back({
                            *contact,
                            collision.first,
                            collision.second,
                            std::fmin(collider1.restitution, collider2.restitution)
                        });
                        // LG_D(
                        //     "Colliding entities: {} and {} (penetration {}, "
                        //     "normal [{}, {}])",
                        //     collision.first,
                        //     collision.second,
                        //     contact->penetration,
                        //     contact->normal.x,
                        //     contact->normal.y);
                    }
                }
                for (int i = 0; i < kContactSolverIterations; ++i)
                {
                    for (const auto& contactInfo : sector->contactInfos)
                    {
                        auto& contact = contactInfo.contact;
                        auto& phy1 = reg->get<PhysicsBody>(contactInfo.ent1);
                        auto& phy2 = reg->get<PhysicsBody>(contactInfo.ent2);
                        auto& transform1 = reg->get<Transform>(contactInfo.ent1);
                        auto& transform2 = reg->get<Transform>(contactInfo.ent2);

                        // One-shot projection: penetration in contact is from the
                        // pre-solve narrowphase pass only; do not re-apply per iter.
                        if (i == 0)
                        {
                            const vec2 correction =
                                contact.normal * contact.penetration * 0.5f;
                            transform1.pos -= correction;
                            transform2.pos += correction;
                        }

                        const vec2 rv = phy2.vel - phy1.vel;
                        const float velAlongNormal =
                            glm::dot(rv, contact.normal);

                        const float invMass1 = 1.0f / phy1.mass;
                        const float invMass2 = 1.0f / phy2.mass;
                        const float denom = invMass1 + invMass2;
                        if (denom < 1e-12f)
                        {
                            continue;
                        }

                        float penBias = 0.0f;
                        if (i == 0 && contact.penetration > kContactPenetrationSlop)
                        {
                            const float invDt =
                                dt > 1e-8f ? 1.0f / dt : 0.0f;
                            penBias =
                                kContactBaumgarte * invDt
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
                        phy1.vel -= impulse * invMass1;
                        phy2.vel += impulse * invMass2;
                    }
                }
            }},
    .afterEntityUpdate = true,
};

}  // namespace ecs

#endif