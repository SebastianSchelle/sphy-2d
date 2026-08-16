#ifndef OBJB_PROJECTILE_HPP
#define OBJB_PROJECTILE_HPP

#include "comp-gfx.hpp"
#include "comp-ident.hpp"
#include "comp-lifetime.hpp"
#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "lib-modules.hpp"
#include "lib-projectile.hpp"
#include "std-inc.hpp"
#include <comp-turret.hpp>
#include <lib-projectile.hpp>
#include <mod-manager.hpp>
#include <objb-general.hpp>
#include <world.hpp>

namespace objb
{

struct Projectile
{
  public:
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::ProjectileHandle& projectileHandle,
                      const ecs::EntityId exceptEntity)
    {
        OBJB_GUARD(Selectable::build(ptrHandle, params), "")
        auto projectile =
            ptrHandle->modManager->getProjectileLib().getItem(projectileHandle);
        OBJB_GUARD(projectile,
                   "Could not find projectile info for {}",
                   projectileHandle.toGenericHandle())
        params.reg.emplace_or_replace<ecs::Projectile>(
            params.entity,
            ecs::Projectile{.projectileHandle =
                                projectileHandle.toGenericHandle()});
        OBJB_GUARD(Lifetime::build(ptrHandle, params, projectile->lifetime), "")
        OBJB_GUARD(Textures::build(ptrHandle, params, projectile->textures),
                   "Projectile texture not built")
        OBJB_GUARD(Collider::build(ptrHandle,
                                   params,
                                   projectile->collider,
                                   ecs::CollisionLayer::Projectile,
                                   exceptEntity),
                   "Projectile collider not built")
        OBJB_GUARD(Physics::build(ptrHandle,
                                  params,
                                  ecs::PhysicsBody{.mass = 1.0f,
                                                   .vel = vec2(0.0f, 0.0f),
                                                   .acc = vec2(0.0f, 0.0f),
                                                   .inertia = 1.0f,
                                                   .rotVel = 0.0f,
                                                   .rotAcc = 0.0f}),
                   "")
        return true;
    }
};

}  // namespace objb

#endif