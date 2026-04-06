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

const System sysPhyPid = {
    "sysPhyPid",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
    {
        // I don't like this crap
        /*
        - calculate direction vector and corresponding max thrust vector
        - from max thrust vector, distance and mass calcute max velocity (clamp
        with maxVel from thrust system)
        - PID control thrust with [-1,1] in x/y direction and scale with thrust
        - Use from hive2d_v2, this is exactly what i have done already / use
        sqrtf

        */

        auto reg = ptrHandle->registry;
        auto* pid = reg->try_get<PhyPid>(entity);
        auto* transform = reg->try_get<Transform>(entity);
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        auto* phyThrust = reg->try_get<PhyThrust>(entity);
        if (!transform || !physicsBody || !phyThrust)
        {
            return;
        }

        if (!pid->active)
        {
            return;
        }
        float angleErr = hmath::angleError(pid->spRot, transform->rot);
        if (physicsBody->inertia == 0)
        {
            return;
        }
        float maxAcc = phyThrust->maxTorque / physicsBody->inertia;
        float desW = 0.7f * sqrtf(2 * maxAcc * abs(angleErr));
        desW *= glm::sign(angleErr);
        float werr = desW - physicsBody->rotVel;
        float trq = phyThrust->maxTorque * ctrl::pdCompute(&pid->pdTurn, dt, werr);
        if (trq == trq)
        {
            phyThrust->setTorque(trq);
        }

        /*glm::vec2 worldDir = pid->spPos - transform->pos;
        float dist = glm::length(worldDir);
        if (dist == 0.0f || physicsBody->mass <= 0.0f)
        {
            return;
        }

        maxAcc = phyThrust->thrustManeuverMax / physicsBody->mass;
        float desVel =
            fminf(phyThrust->maxSpd,
                     sqrtf(2.0f * maxAcc * dist));  //(maxAcc * dist) / (1 + dist);
        glm::vec2 desVelV = worldDir / dist * desVel;

        glm::vec2 err = desVelV - physicsBody->vel;
        float y =
            phyThrust->thrustMainMax * ctrl::pdCompute(&pid->pdFwd, dt, err.y);
        float x =
            phyThrust->thrustMainMax * ctrl::pdCompute(&pid->pdSide, dt, err.x);

        phyThrust->setThrustGlobal(glm::vec2(x, y), *transform);*/
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