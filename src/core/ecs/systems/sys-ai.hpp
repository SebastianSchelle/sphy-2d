#ifndef SYS_AI_HPP
#define SYS_AI_HPP

#include <ecs.hpp>

namespace ecs
{

const System sysAi = {
    .name = "sysAi",
    .type = SystemType::SectorForeachEntitiy,
    .function = SFSectorForeach{
        [](world::Sector* sector, entt::entity entity, const ecs::EntityId& entityId, float dt, PtrHandle* ptrHandle)
    }
};

}  // namespace ecs

#endif