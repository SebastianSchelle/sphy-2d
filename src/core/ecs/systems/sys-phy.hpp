#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include <algorithm>
#include <cmath>
#include <components/comp-ident.hpp>
#include <components/comp-phy.hpp>
#include <ecs.hpp>
#include <std-inc.hpp>
#include <world.hpp>

namespace ecs
{

const float velMargin = 0.8f;
const float posDeadband = 0.1f;
const float velDeadband = 0.04f;
const float rotDeadband = 0.03f;
const float rotVelDeadband = 0.03f;

const System sysMoveCtrl = {
    "sysMoveCtrl",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
    {
        auto reg = ptrHandle->registry;
        auto* sectorId = reg->try_get<ecs::SectorId>(entity);
        auto* moveCtrl = reg->try_get<MoveCtrl>(entity);
        auto* transform = reg->try_get<Transform>(entity);
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        auto* phyThrust = reg->try_get<PhyThrust>(entity);
        if (!moveCtrl || !transform || !physicsBody || !phyThrust || !sectorId
            || !moveCtrl->active || dt <= 1e-6f)
        {
            return;
        }

        // Thrust control =====================
        vec2 relTargetPos = (vec2(moveCtrl->spPos.pos.x, moveCtrl->spPos.pos.y)
                             - vec2(sectorId->x, sectorId->y))
                                * ptrHandle->world->getWorldShape().sectorSize
                            + moveCtrl->spPos.sectorPos;
        const vec2 worldDir = relTargetPos - transform->pos;
        const float dist = length(worldDir);
        const float speed = length(physicsBody->vel);
        const bool inPosDeadzone = dist < posDeadband && speed < velDeadband;
        vec2 dir;
        if (inPosDeadzone)
        {
            phyThrust->setThrustGlobal(vec2(0.0f), *transform);
        }
        else if (dist > 1e-8f)
        {
            glm::vec2 desVelV(0.0f);
            const float maxAcc = fmaxf(
                0.0f,
                std::min(phyThrust->thrustManeuverMax, phyThrust->thrustMainMax)
                    / physicsBody->mass);
            if (maxAcc > 0.0f)
            {
                dir = worldDir / dist;
                const float desVelMag = velMargin * sqrtf(2.0f * maxAcc * dist);
                const float desVel = fminf(phyThrust->maxSpd, desVelMag);
                desVelV = dir * desVel;
            }

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
        if (!inRotDeadzone)
        {
            const float maxAngAcc = phyThrust->maxTorque / physicsBody->inertia;
            const float desWMag =
                velMargin
                * std::sqrt(
                    std::max(0.0f, 2.0f * maxAngAcc * std::abs(angleErr)));
            const float maxRotVel = std::max(0.0f, phyThrust->maxRotVel);
            desW = std::min(desWMag, maxRotVel);
            desW *= glm::sign(angleErr);
        }
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
    }};

const System sysPhyThrust = {
    "sysPhyThrust",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
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
    }};

const System sysPhysics = {
    "sysPhysics",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
    {
        auto reg = ptrHandle->registry;
        auto* sectorId = reg->try_get<ecs::SectorId>(entity);
        auto* transform = reg->try_get<Transform>(entity);
        auto* transformCache = reg->try_get<TransformCache>(entity);
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        if (transform && transformCache && physicsBody)
        {
            physicsBody->acc += -ptrHandle->linDrag * physicsBody->vel;
            physicsBody->rotAcc += -ptrHandle->angDrag * physicsBody->rotVel;
            physicsBody->vel += physicsBody->acc * dt;
            physicsBody->rotVel += physicsBody->rotAcc * dt;
            transform->pos += physicsBody->vel * dt;
            transform->rot += physicsBody->rotVel * dt;
            if(fabsf(physicsBody->rotVel) > 1e-5f)
            {
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

            // LG_D("PhysicsBody update for entity: {} PhysicsBody: {}", entity,
            // *physicsBody); LG_D("Transform update for entity: {} Transform:
            // {}", entity, *transform);
        }
    }};

}  // namespace ecs

#endif