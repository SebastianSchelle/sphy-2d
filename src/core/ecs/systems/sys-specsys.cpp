#include "free-vector.hpp"
#include "logging.hpp"
#include <projectile.hpp>
#include <sys-specsys.hpp>
#include <world.hpp>

namespace ecs
{

void sysProjPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    sector->foreachProj(
        [ptrHandle, dt](specsys::Projectile& projectile, specsys::ProjectileHandle handle)
        {
            projectile.lifetime += dt;
            if(projectile.lifetime > projectile.lifetimeMax)
            {
                return con::FreeVecForeachRet::DESTROY;
            }

            auto& ws = ptrHandle->world->getWorldShape();
            auto& trans = projectile.transform;
            trans.pos += projectile.vel * dt;

            const float ws2 = ws.sectorSize / 2.0f;
            if (trans.pos.x < -ws2 || trans.pos.x > ws2 || trans.pos.y < -ws2
                || trans.pos.y > ws2)
            {
                return con::FreeVecForeachRet::DESTROY;
            }
            return con::FreeVecForeachRet::OK;
        });
}

}  // namespace ecs
