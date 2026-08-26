#ifndef SYS_SPECSYS_HPP
#define SYS_SPECSYS_HPP

#include <std-inc.hpp>
#include <sys-defs.hpp>
#include <world.hpp>

namespace ecs
{

void sysProjPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysProjPhysics = {.name = "sysProjPhysics",
                               .sysFlags = SystemFlags::ActiveSector,
                               .function = sysProjPhysicsImpl};

void sysBeamPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysBeamPhysics = {.name = "sysBeamPhysics",
                               .sysFlags = SystemFlags::ActiveSector,
                               .function = sysBeamPhysicsImpl};

void sysItemPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysItemPhysics = {.name = "sysItemPhysics",
                               .sysFlags = SystemFlags::ActiveSector,
                               .function = sysItemPhysicsImpl};
}  // namespace ecs

#endif