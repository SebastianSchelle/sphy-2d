#include "free-vector.hpp"
#include <projectile.hpp>
#include <sys-specsys.hpp>
#include <world.hpp>

namespace ecs
{

void sysProjPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    ptrHandle->projPool->foreach (
        [ptrHandle](specsys::Projectile& projectile)
        {
            auto& ws = ptrHandle->world->getWorldShape();
            auto& trans = projectile.transform;
            trans.pos += projectile.vel;

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
