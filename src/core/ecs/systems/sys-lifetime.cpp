#include <sys-lifetime.hpp>

namespace ecs
{

void sysLifetimeImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    auto* reg = sector->getRegistry()->getRegistry();
    reg->view<EntityId, Lifetime>().each(
        [ptrHandle, sector, dt](auto entity, auto& entityId, auto& lifetime)
        {
            lifetime.lifetime -= dt;
            if (lifetime.lifetime <= 0.0f)
            {
                sector->markEntityForDestruction(ptrHandle, entityId);
            }
        });
}

}  // namespace ecs