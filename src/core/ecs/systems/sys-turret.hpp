#ifndef SYS_TURRET_HPP
#define SYS_TURRET_HPP

#include <comp-struct.hpp>
#include <comp-turret.hpp>
#include <ecs.hpp>
#include <lib-modules.hpp>
#include <mod-manager.hpp>

namespace ecs
{

void sysTurretImpl(world::Sector* sector,
               const entt::entity entity,
               const ecs::EntityId& entityId,
               const float dt,
               PtrHandle* ptrHandle);

const System sysTurret = {
    .name = "sysTurret",
    .type = SystemType::SectorForeachEntitiy,
    .function = SFSectorForeach{
        sysTurretImpl
    }
};

}  // namespace ecs

#endif  // SYS_TURRET_HPP