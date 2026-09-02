#ifndef CLIENT_POOL_OBJ_HPP
#define CLIENT_POOL_OBJ_HPP

#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "free-vector.hpp"
#include "std-inc.hpp"
#include <lib-projectile.hpp>

namespace opool
{

struct vec2Mixer
{
    vec2 pos;

    struct ExtraParam
    {
    };
    vec2Mixer mix(const vec2Mixer& other, float alpha, const ExtraParam& extra) const
    {
        const vec2 mixPos = glm::mix(pos, other.pos, alpha);
        return {.pos = mixPos};
    }
};

struct LineMixer
{
    vec2 pos1;
    vec2 pos2;

    struct ExtraParam
    {
    };
    LineMixer mix(const LineMixer& other, float alpha, const ExtraParam& extra) const
    {
        const vec2 mixPos1 = glm::mix(pos1, other.pos1, alpha);
        const vec2 mixPos2 = glm::mix(pos2, other.pos2, alpha);
        return {mixPos1, mixPos2};
    }
};

struct ProjClient
{
    struct Params
    {
        ecs::Transform tr;
        long time;
        gobj::ProjectileHandle proj;
    };
    InterpolData<vec2Mixer> pos;
    float rot;
    gobj::ProjectileHandle proj;

    ProjClient() {}
    ProjClient(const Params& p) : rot(p.tr.rot), proj(p.proj)
    {
        pos.addSample({p.tr.pos}, p.time);
    }
    void update(const Params& p)
    {
        pos.addSample({p.tr.pos}, p.time);
    }
};
using ProjHandleClient = typename con::FreeVec<ProjClient>::Handle;

struct BeamClient
{
    struct Params
    {
        vec2 p1;
        vec2 p2;
        long time;
        gobj::BeamHandle beam;
    };
    InterpolData<LineMixer> line;
    gobj::BeamHandle beam;

    BeamClient() {}
    BeamClient(const Params& p) : beam(p.beam)
    {
        line.addSample({p.p1, p.p2}, p.time);
    }
    void update(const Params& p)
    {
        line.addSample({p.p1, p.p2}, p.time);
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
        long time;
    };
    InterpolData<vec2Mixer> pos;
    float rot;
    gobj::ItemHandle item;
    uint32_t quantity;

    ItemClient() {}
    ItemClient(const Params& p)
        : rot(p.transform.rot), item(p.item), quantity(p.quantity)
    {
        pos.addSample({p.transform.pos}, p.time);
    }
    void update(const Params& p)
    {
        pos.addSample({p.transform.pos}, p.time);
        quantity = p.quantity;
    }
};
using ItemHandleClient = typename con::FreeVec<ItemClient>::Handle;

}  // namespace opool

#endif