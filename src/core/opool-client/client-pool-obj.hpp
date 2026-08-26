#ifndef CLIENT_POOL_OBJ_HPP
#define CLIENT_POOL_OBJ_HPP

#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "free-vector.hpp"
#include <lib-projectile.hpp>

namespace opool
{

struct TimedPos
{
    tim::Timepoint t;
    vec2 pos;
};

struct TimedTr
{
    tim::Timepoint t;
    ecs::Transform tr;
};

struct ProjClient
{
    struct Params
    {
        ecs::Transform tr;
        gobj::ProjectileHandle proj;
    };
    bool hasPrev;
    TimedPos posPrev;
    TimedPos posNext;
    float rot;
    gobj::ProjectileHandle proj;

    ProjClient() {}
    ProjClient(const Params& p)
        : rot(p.tr.rot), hasPrev(false), proj(p.proj), posNext{.pos = p.tr.pos}
    {
    }
    void update(const Params& p)
    {
        hasPrev = true;
        posPrev = posNext;
        posNext = {.pos = p.tr.pos};
    }
};
using ProjHandleClient = typename con::FreeVec<ProjClient>::Handle;

struct BeamClient
{
    struct Params
    {
        ecs::Transform tr;
        gobj::BeamHandle beam;
    };
    bool hasPrev;
    TimedTr trPrev;
    TimedTr trNext;
    gobj::BeamHandle beam;

    BeamClient() {}
    BeamClient(const Params& p)
        : hasPrev(false), beam(p.beam), trNext{.tr = p.tr}
    {
    }
    void update(const Params& p)
    {
        hasPrev = true;
        trPrev = trNext;
        trNext = {.tr = p.tr};
    }
};
using BeamHandleClient = typename con::FreeVec<BeamClient>::Handle;

struct ItemClient
{
    struct Params
    {
        ecs::Transform transform;
        gobj::ItemHandle item;
        uint32_t quantity;
    };
    ecs::Transform transform;
    gobj::ItemHandle item;
    uint32_t quantity;

    ItemClient() {}
    ItemClient(const Params& p)
        : transform(p.transform), item(p.item), quantity(p.quantity)
    {
    }
    void update(const Params& p)
    {
        transform = p.transform;
        quantity = p.quantity;
    }
};
using ItemHandleClient = typename con::FreeVec<ItemClient>::Handle;

}  // namespace opool

#endif