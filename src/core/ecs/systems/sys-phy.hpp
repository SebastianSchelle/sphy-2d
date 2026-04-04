#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include <cmath>
#include <components/comp-phy.hpp>
#include <ecs.hpp>
#include <std-inc.hpp>

namespace ecs
{

const System sysPhysics = {
    "sysPhysics",
    [](entt::entity entity, float dt, std::shared_ptr<PtrHandle> ptrHandle)
    {
        auto reg = ptrHandle->registry;
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

            // Reset acceleration after game update
            physicsBody->acc = {0, 0};
            physicsBody->rotAcc = 0;

            // LG_D("PhysicsBody update for entity: {} PhysicsBody: {}", entity, *physicsBody);
            // LG_D("Transform update for entity: {} Transform: {}", entity, *transform);
        }
    }};

const System sysPhyThrust = {
    "sysPhyThrust",
    [](entt::entity entity, float dt, std::shared_ptr<PtrHandle> ptrHandle)
    {
        auto reg = ptrHandle->registry;
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        auto* phyThrust = reg->try_get<PhyThrust>(entity);
        if (phyThrust && physicsBody)
        {
            if (physicsBody->rotVel > phyThrust->maxRotVel && phyThrust->torque > 0) {
                phyThrust->torque = 0.0;
            } else if (physicsBody->rotVel < -phyThrust->maxRotVel && phyThrust->torque < 0) {
                phyThrust->torque = 0.0;
            }
            physicsBody->rotAcc += phyThrust->torque / physicsBody->inertia;
        
            float spd = glm::length(physicsBody->vel);
            glm::vec2 thrustApply = phyThrust->thrustGlobal;
            if (spd >= phyThrust->maxSpd && spd > 1e-8f) {
                const glm::vec2 vhat = physicsBody->vel / spd;
                const float along = glm::dot(phyThrust->thrustGlobal, vhat);
                if (along > 0.f) {
                    thrustApply -= vhat * along;
                }
            }
            physicsBody->acc += thrustApply / physicsBody->mass;
        
            // Reset thrust after each update cycle
            phyThrust->torque = 0.0f;
            phyThrust->thrustGlobal = vec2(0.0f);
            phyThrust->thrustLocal = vec2(0.0f);

            // LG_D("PhyThrust update for entity: {} PhyThrust: {}", entity, *phyThrust);
        }
    }};

}  // namespace ecs

#endif