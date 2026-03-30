#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include <cmath>
#include <std-inc.hpp>
#include <ecs.hpp>
#include <components/comp-phy.hpp>

namespace ecs
{

const System sysPhysics = {
    "sysPhysics",
    [](entt::entity entity, float dt, std::shared_ptr<PtrHandle> ptrHandle) {
        auto reg = ptrHandle->registry;
        auto* transform = reg->try_get<Transform>(entity);
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        if (transform && physicsBody)
        {
            physicsBody->vel += physicsBody->acc * dt;
            physicsBody->rotVel += physicsBody->rotAcc * dt;
            transform->pos += physicsBody->vel * dt;
            transform->rot += physicsBody->rotVel * dt;
            if(transform->rot < 0.0f)
            {
                transform->rot += 2.0f * M_PIf;
            }
            else if(transform->rot >= 2.0f * M_PIf)
            {
                transform->rot -= 2.0f * M_PIf;
            }

            // Reset acceleration after game update
            physicsBody->acc = {0, 0};
            physicsBody->rotAcc = 0;

            //LG_D("Physics update for entity: {} Position: {}, Rotation: {}", entity, transform->pos, transform->rot);
        }
    }
};

}

#endif