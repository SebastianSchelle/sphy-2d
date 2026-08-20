#ifndef PROJECTILE_HPP
#define PROJECTILE_HPP

#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "free-vector.hpp"
#include "lib-projectile.hpp"

namespace specsys
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

class ProjectilePool
{
  public:
    ProjectilePool() {}
    ~ProjectilePool() {}
    ProjectileHandle spawnProjectile(gobj::ProjectileHandle proj,
                                     vec2 vel,
                                     const ecs::Transform& tr,
                                     float lifetime,
                                     ecs::EntityId collExcept = ecs::EntityId::Invalid());
    void destroyProjectile(ProjectileHandle handle);
    void foreach (
        std::function<con::FreeVecForeachRet(Projectile&,
                                             ProjectileHandle handle)> clb);

  private:
    con::FreeVec<Projectile> pool;
};

}  // namespace specsys

#endif