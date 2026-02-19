#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include <std-inc.hpp>
#include <ecs.hpp>
#include <ptr-handle.hpp>
#include <components/comp-phy.hpp>

namespace ecs
{

const System sysPhysics = {
    "sysPhysics",
    [](entt::entity entity, float dt, std::shared_ptr<PtrHandle> ptrHandle) {
        auto* transform = ptrHandle->registry.try_get<Transform>(entity);
        auto* physicsBody = ptrHandle->registry.try_get<PhysicsBody>(entity);
        if (transform && physicsBody)
        {
            physicsBody->vel += physicsBody->acc * dt;
            physicsBody->rotVel += physicsBody->rotAcc * dt;
            transform->pos += physicsBody->vel * dt;
            transform->rot += physicsBody->rotVel * dt;

            // Reset acceleration after game update
            physicsBody->acc = {0, 0};
            physicsBody->rotAcc = 0;
        }
    }
};

}

#endif