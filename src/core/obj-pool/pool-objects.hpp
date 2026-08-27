#ifndef POOL_OBJECTS_HPP
#define POOL_OBJECTS_HPP

#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "free-vector.hpp"
#include "lib-projectile.hpp"
#include "std-inc.hpp"

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
    static constexpr int32_t INVALID_PROXY_ID = -1;
    static constexpr float ITEM_TEX_SIZE = 0.5f * 15.0f * gfx::kTexturePixelToWorld;
    static constexpr vec2 HalfSize = vec2(ITEM_TEX_SIZE, ITEM_TEX_SIZE);

    ecs::Transform transform;
    gobj::ItemHandle item;
    uint32_t quantity;
    vec2 vel;
    ecs::EntityId collExcept;
    float lifetimeMax;
    float lifetime = 0.0f;
    int32_t proxyId = INVALID_PROXY_ID;
    bool reserved;
};
using ItemHandle = typename con::FreeVec<Item>::Handle;

struct Beam
{
    ecs::Transform origin;
    vec2 point2;
    ecs::EntityId collExcept;
    gobj::BeamHandle beam;
};
using BeamHandle = typename con::FreeVec<Beam>::Handle;

}  // namespace opool

#endif