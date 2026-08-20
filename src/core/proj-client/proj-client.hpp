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
    float rot;
    gobj::ProjectileHandle proj;
};
using ProjHandleClient = typename con::FreeVec<ProjClient>::Handle;

// todo: make this generalised so several systems can be exchanged e.g.
// missiles/projectiles/...
class Projectiles
{
  public:
    Projectiles() {}
    ~Projectiles() {}
    void markInactive();
    void deleteInactive();
    void updateProjectile(const GenericHandle32& handle,
                          gobj::ProjectileHandle proj,
                          //tim::Timepoint t,
                          vec2 pos,
                          float rot);
    void foreach(std::function<void(ProjClient& proj)> clb);

  private:
    unordered_map<uint32_t, ProjClient> projectiles;
};

}  // namespace specsys

#endif
