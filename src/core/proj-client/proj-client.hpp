#ifndef PROJ_CLIENT_HPP
#define PROJ_CLIENT_HPP

#include "lib-projectile.hpp"
#include "std-inc.hpp"
#include <free-vector.hpp>

namespace specsys
{

struct TimedPos
{
    tim::Timepoint t;
    vec2 pos;
};

struct ProjClient
{
    uint16_t generation;
    bool active;
    bool hasPrev;
    TimedPos posPrev;
    TimedPos posNext;
    gobj::ProjectileHandle proj;
};
using ProjHandleClient = typename con::FreeVec<ProjClient>::Handle;

// todo: make this generalised so several systems can be exchanged e.g. missiles/projectiles/...
class Projectiles
{
  public:
    Projectiles() {}
    ~Projectiles() {}
    void markInactive();
    void deleteInactive();
    void updateProjectile(uint32_t idx,
                          uint16_t gen,
                          gobj::ProjectileHandle proj,
                          tim::Timepoint t,
                          vec2 pos);

  private:
    unordered_map<uint32_t, ProjClient> projectiles;
};

}  // namespace specsys

#endif
