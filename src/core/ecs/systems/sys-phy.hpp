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

const float kpTurn = 0.15f;
const float velMargin = 0.5f;
const float inertiaRef = 10.0f;

const System sysPhyPid = {
    "sysPhyPid",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
    {
        auto reg = ptrHandle->registry;
        auto* pid = reg->try_get<PhyPid>(entity);
        auto* transform = reg->try_get<Transform>(entity);
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        auto* phyThrust = reg->try_get<PhyThrust>(entity);
        if (!pid || !transform || !physicsBody || !phyThrust || dt <= 1e-6f
            || !pid->active)
        {
            return;
        }

        // Torque control
        if (physicsBody->inertia > 1e-8f && phyThrust->maxTorque > 0.0f)
        {
            const float angleErr =
                hmath::angleError(pid->spRot, transform->rot);
            const float maxAngAcc = phyThrust->maxTorque / physicsBody->inertia;
            const float desWMag =
                velMargin
                * std::sqrt(
                    std::max(0.0f, 2.0f * maxAngAcc * std::abs(angleErr)));
            const float maxRotVel = std::max(0.0f, phyThrust->maxRotVel);
            float desW = std::min(desWMag, maxRotVel);
            desW *= glm::sign(angleErr);
#ifdef DEBUG
            pid->spRotVel = desW;
#endif
            const float werr = desW - physicsBody->rotVel;
            float trqNorm = kpTurn * werr;
            trqNorm *= physicsBody->inertia / inertiaRef;
            const float trq = phyThrust->maxTorque * trqNorm;
            phyThrust->setTorque(trq);
        }

        // Thrust control
        /*
                if (physicsBody->mass <= 1e-8f)
                {
                    return;
                }

                // Translation: stop-distance speed profile and PD on velocity.
                const glm::vec2 worldDir = pid->spPos - transform->pos;
                const float dist = glm::length(worldDir);
                const float speed = glm::length(physicsBody->vel);
                glm::vec2 desVelV(0.0f);
                // if (dist > kPosDeadband)
                // {
                const float maxAcc = std::max(
                    0.0f,
                    std::min(phyThrust->thrustManeuverMax,
           phyThrust->thrustMainMax) / physicsBody->mass); if (maxAcc > 0.0f)
                {
                    const float desVel =
                        std::min(phyThrust->maxSpd, std::sqrt(2.0f * maxAcc *
           dist)); desVelV = worldDir / dist * desVel; pid->spVelX = desVelV.x;
                    pid->spVelY = desVelV.y;
                }
                //}
                // else if (speed < kVelDeadband)
                // {
                //     pid->pdFwd.prev_error = 0.0f;
                //     pid->pdSide.prev_error = 0.0f;
                //     return;
                //}

                const glm::vec2 err = desVelV - physicsBody->vel;
                const float y =
                    phyThrust->thrustMainMax * ctrl::pdCompute(&pid->pdFwd, dt,
           err.y); const float x = phyThrust->thrustManeuverMax
                                * ctrl::pdCompute(&pid->pdSide, dt, err.x);
                if (std::isfinite(x) && std::isfinite(y))
                {
                    phyThrust->setThrustGlobal(glm::vec2(x, y), *transform);
                }
        */
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
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        if (transform && physicsBody)
        {
            physicsBody->vel += physicsBody->acc * dt;
            physicsBody->rotVel += physicsBody->rotAcc * dt;
            transform->pos += physicsBody->vel * dt;
            transform->rot += physicsBody->rotVel * dt;
            if (transform->rot < 0.0f)
            {
                transform->rot += 2.0f * M_PIf;
            }
            else if (transform->rot >= 2.0f * M_PIf)
            {
                transform->rot -= 2.0f * M_PIf;
            }

            // Check for sector switch
            ptrHandle->world->checkSectorSwitchAfterMove(
                entityId, entity, sectorId, transform, ptrHandle);

            // Reset acceleration after game update
            physicsBody->acc = {0, 0};
            physicsBody->rotAcc = 0;

            // LG_D("PhysicsBody update for entity: {} PhysicsBody: {}", entity,
            // *physicsBody); LG_D("Transform update for entity: {} Transform:
            // {}", entity, *transform);
        }
    }};

}  // namespace ecs

#endif