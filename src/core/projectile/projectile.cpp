#include "projectile.hpp"

namespace specsys
{

ProjectileHandle ProjectilePool::spawnProjectile(gobj::ProjectileHandle proj,
                                                 vec2 vel,
                                                 const ecs::Transform& tr)
{
    auto handle = pool.addItem(Projectile(proj, vel, tr));
    return handle;
}

void ProjectilePool::destroyProjectile(ProjectileHandle handle)
{
    pool.removeItem(handle);
}

void ProjectilePool::foreach (
    std::function<con::FreeVecForeachRet(Projectile&)> clb)
{
    pool.foreach (clb);
}

}  // namespace specsys