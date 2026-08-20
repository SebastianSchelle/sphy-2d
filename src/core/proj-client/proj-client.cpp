#include "std-inc.hpp"
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

void Projectiles::updateProjectile(const GenericHandle32& handle,
                                   gobj::ProjectileHandle proj,
                                   // tim::Timepoint t,
                                   vec2 pos,
                                   float rot)
{
    auto it = projectiles.find(handle.idx);
    if (it != projectiles.end())
    {
        auto& item = it->second;
        item.active = true;
        if (item.generation == handle.gen)
        {
            item.hasPrev = false;
            item.posPrev = item.posNext;
            item.posNext = {.pos = pos};
            // LG_D("Proj moved to {}", item.posNext.pos);
        }
        else
        {
            // New projectile on previously occupied slot
            item.generation = handle.gen;
            item.hasPrev = false;
            item.posPrev = item.posNext;
            item.posNext = {.pos = pos};
            item.proj = proj;
            item.rot = rot;
        }
    }
    else
    {
        projectiles[handle.idx] = ProjClient{.generation = handle.gen,
                                             .active = true,
                                             .hasPrev = false,
                                             .posNext = {.pos = pos},
                                             .rot = rot,
                                             .proj = proj};
    }
}

void Projectiles::foreach (std::function<void(ProjClient& proj)> clb)
{
    for (auto& item : projectiles)
    {
        if (item.second.active)
        {
            clb(item.second);
        }
    }
}

}  // namespace specsys
