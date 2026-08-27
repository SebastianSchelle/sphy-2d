#ifndef SYS_TURRET_HPP
#define SYS_TURRET_HPP

#include <comp-struct.hpp>
#include <comp-turret.hpp>
#include <lib-modules.hpp>
#include <mod-manager.hpp>

namespace ecs
{

void sysTurretImpl(world::Sector* sector, const float dt, PtrHandle* ptrHandle);

const System sysTurret = {.name = "sysTurret",
                          .sysFlags = SystemFlags::ActiveSector,
                          .function = sysTurretImpl};

void sysCollectorImpl(world::Sector* sector,
                      const float dt,
                      PtrHandle* ptrHandle);

const System sysCollector = {.name = "sysCollector",
                          .sysFlags = SystemFlags::ActiveSector,
                          .function = sysCollectorImpl};

}  // namespace ecs

#endif  // SYS_TURRET_HPP