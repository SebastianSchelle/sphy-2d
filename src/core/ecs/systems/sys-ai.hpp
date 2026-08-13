#ifndef SYS_AI_HPP
#define SYS_AI_HPP

#include "sys-defs.hpp"
#include <components/comp-ai.hpp>
#include <ecs.hpp>
#include <sector.hpp>
#include <task-system.hpp>

namespace ecs
{

void sysAiImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysAi = {.name = "sysAi",
                      .sysFlags = SystemFlags::ActiveSector,
                      .function = sysAiImpl};

}  // namespace ecs

#endif