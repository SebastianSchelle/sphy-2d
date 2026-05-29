#ifndef SYS_LIFETIME_HPP
#define SYS_LIFETIME_HPP

#include <components/comp-lifetime.hpp>
#include <ecs.hpp>
#include <sector.hpp>

namespace ecs
{

const System sysLifetime = {
    .name = "sysLifetime",
    .type = SystemType::SectorForeachEntitiy,
    .function = SFSectorForeach{
        [](world::Sector* sector,
           const entt::entity entity,
           const ecs::EntityId& entityId,
           const float dt,
           PtrHandle* ptrHandle)
        {
            auto reg = ptrHandle->registry;
            auto* lifetime = reg->try_get<Lifetime>(entity);
            if (lifetime)
            {
                lifetime->lifetime -= dt;
                if (lifetime->lifetime <= 0.0f)
                {
                    sector->markEntityForDestruction(entityId);
                }
            }
        }}};

}  // namespace ecs

#endif // SYS_LIFETIME_HPP