#ifndef POOL_OBJECTS_HPP
#define POOL_OBJECTS_HPP

#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "comp-struct.hpp"
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

struct Item
{
    ecs::Transform transform;
    gobj::ItemHandle item;
    uint32_t quantity;
    vec2 vel;
    ecs::EntityId collExcept;
    float lifetimeMax;
    float lifetime = 0.0f;
};
using ItemHandle = typename con::FreeVec<Item>::Handle;

}  // namespace opool

#endif