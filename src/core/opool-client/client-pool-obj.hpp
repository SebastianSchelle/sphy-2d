#ifndef CLIENT_POOL_OBJ_HPP
#define CLIENT_POOL_OBJ_HPP

#include "comp-phy.hpp"
#include "free-vector.hpp"
#include <lib-projectile.hpp>

namespace opool
{

struct TimedPos
{
    tim::Timepoint t;
    vec2 pos;
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
    ProjClient(Params p)
        : rot(p.tr.rot), hasPrev(false), proj(p.proj), posNext{.pos = p.tr.pos}
    {
    }
    void update(Params p)
    {
        hasPrev = true;
        posPrev = posNext;
        posNext = {.pos = p.tr.pos};
    }
};
using ProjHandleClient = typename con::FreeVec<ProjClient>::Handle;

}  // namespace opool

#endif