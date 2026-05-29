#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include <ecs.hpp>
#include <std-inc.hpp>
#include <world.hpp>

namespace ecs
{

void sysMoveCtrlImpl(world::Sector* sector,
                     entt::entity entity,
                     const ecs::EntityId& entityId,
                     float dt,
                     PtrHandle* ptrHandle);

const System sysMoveCtrl = {.name = "sysMoveCtrl",
                            .type = SystemType::SectorForeachEntitiy,
                            .function = sysMoveCtrlImpl};

void sysPhyThrustImpl(world::Sector* sector,
                      entt::entity entity,
                      const ecs::EntityId& entityId,
                      float dt,
                      PtrHandle* ptrHandle);

const System sysPhyThrust = {.name = "sysPhyThrust",
                             .type = SystemType::SectorForeachEntitiy,
                             .function = sysPhyThrustImpl};

void sysPhysicsImpl(world::Sector* sector,
                    entt::entity entity,
                    const ecs::EntityId& entityId,
                    float dt,
                    PtrHandle* ptrHandle);

const System sysPhysics = {.name = "sysPhysics",
                           .type = SystemType::SectorForeachEntitiy,
                           .function = sysPhysicsImpl};

void sysCollisionDetectionImpl(world::Sector* sector,
                               float dt,
                               PtrHandle* ptrHandle);

const System sysCollisionDetection = {.name = "sysCollisionDetection",
                                      .type = SystemType::SectorLate,
                                      .function = sysCollisionDetectionImpl};

void sysAnchorFixedImpl(world::Sector* sector,
                        entt::entity entity,
                        const ecs::EntityId& entityId,
                        float dt,
                        PtrHandle* ptrHandle);

const System sysAnchorFixed = {.name = "sysAnchorFixed",
                               .type = SystemType::SectorForeachEntitiy,
                               .function = sysAnchorFixedImpl};

}  // namespace ecs

#endif