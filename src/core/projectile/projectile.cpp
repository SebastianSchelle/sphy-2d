#include "projectile.hpp"
#include "comp-ident.hpp"

namespace specsys
{

ProjectileHandle ProjectilePool::spawnProjectile(
    gobj::ProjectileHandle proj,
    vec2 vel,
    const ecs::Transform& tr,
    float lifetime,
    ecs::EntityId collExcept)
{
    auto handle = pool.addItem(Projectile{.transform = tr,
                                          .collExcept = collExcept,
                                          .proj = proj,
                                          .vel = vel,
                                          .lifetimeMax = lifetime});
    return handle;
}

void ProjectilePool::destroyProjectile(ProjectileHandle handle)
{
    pool.removeItem(handle);
}

void ProjectilePool::foreach (
    std::function<con::FreeVecForeachRet(Projectile&, ProjectileHandle handle)>
        clb)
{
    pool.foreach (clb);
}

}  // namespace specsys