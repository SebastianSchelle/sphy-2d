#include "lib-projectile.hpp"
#include <boost/date_time/posix_time/ptime.hpp>
#include <proj-client.hpp>

namespace specsys
{
void Projectiles::markInactive()
{
    for (auto& it : projectiles)
    {
        auto& item = it.second;
        item.active = false;
    }
}

void Projectiles::deleteInactive()
{
    for (auto it = projectiles.begin(); it != projectiles.end();)
    {
        if (!it->second.active)
        {
            it = projectiles.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void Projectiles::updateProjectile(uint32_t idx,
                                   uint16_t gen,
                                   gobj::ProjectileHandle proj,
                                   tim::Timepoint t,
                                   vec2 pos)
{
    auto it = projectiles.find(idx);
    if (it != projectiles.end())
    {
        auto& item = it->second;
        item.active = true;
        if (item.generation == gen)
        {
            item.hasPrev = false;
            item.posPrev = item.posNext;
            item.posNext = {.t = t, .pos = pos };
        }
        else
        {
            // New projectile on previously occupied slot
            item.generation = gen;
            item.hasPrev = false;
            item.posPrev = item.posNext;
            item.posNext = {.t = t, .pos = pos };
            item.proj = proj;
        }
    }
    else
    {
        projectiles[idx] = ProjClient{
            .generation = gen,
            .active = true,
            .hasPrev = false,
            .posNext = {.t=t, .pos=pos},
            .proj = proj
        };
    }
}

}  // namespace specsys
