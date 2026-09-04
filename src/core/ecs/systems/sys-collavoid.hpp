#ifndef SYS_COLLAVOID_HPP
#define SYS_COLLAVOID_HPP

#include "config-manager.hpp"
#include "sys-defs.hpp"
#include <sector.hpp>

namespace ecs
{

void initCollAvoid(const cfg::ConfigManager& config);
void sysCollAvoidImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysCollAvoid = {.name = "sysCollAvoid",
                             .sysFlags = SystemFlags::ActiveSector,
                             .function = sysCollAvoidImpl};

}  // namespace ecs

#endif