#ifndef POOL_OBJECTS_HPP
#define POOL_OBJECTS_HPP

#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "free-vector.hpp"
#include "lib-projectile.hpp"

namespace opool
{

struct Projectile
{
    ecs::Transform transform;
    ecs::EntityId collExcept;
    gobj::ProjectileHandle proj;
    vec2 vel;
    float lifetimeMax;
    float lifetime = 0.0f;
};
using ProjectileHandle = typename con::FreeVec<Projectile>::Handle;

#define SER_PROJECTILE_2                                                       \
    SOBJ(o.transform);                                                         \
    SOBJ(o.proj.toGenericHandle());

EXT_SER(Projectile, SER_PROJECTILE_2)

}  // namespace opool

#endif