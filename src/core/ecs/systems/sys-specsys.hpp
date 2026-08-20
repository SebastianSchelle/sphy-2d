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

}  // namespace ecs

#endif