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
  public:
    Projectile(gobj::ProjectileHandle proj,
               vec2 vel,
               const ecs::Transform& tr,
               const ecs::EntityId collExc = ecs::EntityId::Invalid())
        : vel(vel), transform(tr), collExcept(collExc), proj(proj)
    {
    }
    ~Projectile(){}

    ecs::Transform transform;
    ecs::EntityId collExcept;
    gobj::ProjectileHandle proj;
    vec2 vel;
};
using ProjectileHandle = typename con::FreeVec<Projectile>::Handle;


class ProjectilePool
{
  public:
    ProjectilePool(){}
    ~ProjectilePool(){}
    ProjectileHandle spawnProjectile(gobj::ProjectileHandle proj,
                                     vec2 vel,
                                     const ecs::Transform& tr);
    void destroyProjectile(ProjectileHandle handle);
    void foreach (std::function<con::FreeVecForeachRet(Projectile&)> clb);

  private:
    con::FreeVec<Projectile> pool;
};

}  // namespace specsys

#endif