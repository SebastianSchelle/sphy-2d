#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include "lib-asteroid.hpp"
#include <std-inc.hpp>
#include <sys-defs.hpp>
#include <world.hpp>
#include <comp-struct.hpp>

namespace ecs
{

void sysMoveCtrlImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysMoveCtrl = {.name = "sysMoveCtrl",
                            .sysFlags = SystemFlags::ActiveSector,
                            .function = sysMoveCtrlImpl};

void sysPhyThrustImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysPhyThrust = {.name = "sysPhyThrust",
                             .sysFlags = SystemFlags::ActiveSector,
                             .function = sysPhyThrustImpl};

void sysPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysPhysics = {.name = "sysPhysics",
                           .sysFlags = SystemFlags::ActiveSector,
                           .function = sysPhysicsImpl};

void sysCollisionDetectionImpl(world::Sector* sector,
                               float dt,
                               PtrHandle* ptrHandle);

const System sysCollisionDetection = {.name = "sysCollisionDetection",
                                      .sysFlags = SystemFlags::ActiveSector,
                                      .function = sysCollisionDetectionImpl};

void sysAnchorFixedImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle);

const System sysAnchorFixed = {.name = "sysAnchorFixed",
                               .sysFlags = SystemFlags::ActiveSector,
                               .function = sysAnchorFixedImpl};

void damageAndMine(ecs::Asteroid& asteroid,
                   PtrHandle* ptrHandle,
                   world::Sector* sector,
                   float dmg,
                   entt::entity ast,
                   const vec2& collPos);

}  // namespace ecs

#endif
