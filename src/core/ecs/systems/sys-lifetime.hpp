#ifndef SYS_LIFETIME_HPP
#define SYS_LIFETIME_HPP

#include "sys-defs.hpp"
#include <comp-lifetime.hpp>
#include <sector.hpp>

namespace ecs
{


void sysLifetimeImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysLifetime = {.name = "sysLifetime",
                            .sysFlags = SystemFlags::ActiveSector
                                        | SystemFlags::InactiveSector,
                            .function = sysLifetimeImpl};

}  // namespace ecs

#endif  // SYS_LIFETIME_HPP