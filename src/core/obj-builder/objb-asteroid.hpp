#ifndef OBJB_ASTEROID_HPP
#define OBJB_ASTEROID_HPP

#include "comp-phy.hpp"
#include "comp-storage.hpp"
#include "entt/entity/fwd.hpp"
#include "lib-asteroid.hpp"
#include <comp-struct.hpp>
#include <mod-manager.hpp>
#include <objb-general.hpp>

namespace objb
{

struct Asteroid
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::AsteroidHandle& asteroidHandle,
                      float rotVel)
    {
        OBJB_GUARD(Selectable::build(ptrHandle, params), "")
        gobj::Asteroid* asteroid =
            ptrHandle->modManager->getAsteroidLib().getItem(asteroidHandle);
        OBJB_GUARD(asteroid,
                   "Asteroid: Could not find asteroid entry for {}",
                   asteroidHandle.toGenericHandle())
        params.reg.emplace_or_replace<ecs::Asteroid>(
            params.entity,
            ecs::Asteroid{.asteroidHandle = asteroidHandle.toGenericHandle(),
                          .volume = asteroid->volume,
                          .harvestProgress = 0.0f});
        OBJB_GUARD(Textures::build(ptrHandle, params, asteroid->textures),
                   "Asteroid: Failed to build textures")
        OBJB_GUARD(Collider::build(ptrHandle,
                                   params,
                                   asteroid->collider,
                                   ecs::CollisionLayer::Ship),
                   "Asteroid: Failed to build collider")
        // colliderData is ensured by Collider::build
        auto* colliderData =
            ptrHandle->modManager->getColliderLib().getItem(asteroid->collider);
        vec2 extends = smath::colliderLocalExtents(colliderData->vertices);
        float inertia =
            smath::approximateInertia(asteroid->volume, extends.x, extends.y);
        OBJB_GUARD(
            Physics::build(ptrHandle,
                           params,
                           ecs::PhysicsBody{.mass = asteroid->volume * 3000.0f,
                                            .vel = vec2(0.0f, 0.0f),
                                            .acc = vec2(0.0f, 0.0f),
                                            .inertia = inertia,
                                            .rotVel = 0.0f,
                                            .naturalRotation = rotVel}),
            "")
        return true;
    }
};

}  // namespace objb

#endif