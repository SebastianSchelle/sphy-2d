#include "task-basic.hpp"
#include <ecs.hpp>
#include <world.hpp>

namespace ai
{
namespace taskdata
{

TaskFunResult Idle::function(TaskFunArgs* args)
{
    return TaskFunResult::Done;
}

TaskFunResult UniversePatrol::function(TaskFunArgs* args)
{
    auto* reg = args->ptrHandle->registry;
    auto* transform = reg->try_get<ecs::Transform>(args->entity);
    auto* sectorId = reg->try_get<ecs::SectorId>(args->entity);
    auto* moveCtrl = reg->try_get<ecs::MoveCtrl>(args->entity);
    if (!transform || !sectorId || !moveCtrl)
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        LG_D("SectorPatrol: EcsCompMissing");
        return TaskFunResult::EcsCompMissing;
    }
    if (!state.initialized)
    {
        makeRandomPos(args);
        state.initialized = true;
    }
    moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::MoveTo;
    moveCtrl->spPos = state.randomPos;
    moveCtrl->allowedPosError = config.allowedPosError;
    moveCtrl->allowedRotError = config.allowedRotError;
    moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::Forward;
    if (moveCtrl->moveMode != ecs::MoveCtrl::MoveMode::None
        && moveCtrl->posReached
        && moveCtrl->turnMode != ecs::MoveCtrl::TurnMode::None
        && moveCtrl->rotReached)
    {
        moveCtrl->posReached = false;
        moveCtrl->rotReached = false;
        moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::None;
        moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::None;
        makeRandomPos(args);
    }
    SCHED_NEXT(DEFAULT_INTERVAL);
    return TaskFunResult::Continue;
}

void UniversePatrol::makeRandomPos(TaskFunArgs* args)
{
    auto& worldShape = args->ptrHandle->world->getWorldShape();
    state.randomPos.sectorPos =
        0.9f
        * vec2(
            (rand() % (int)worldShape.sectorSize) - worldShape.sectorSize / 2,
            (rand() % (int)worldShape.sectorSize) - worldShape.sectorSize / 2);
    state.randomPos.pos.x = (rand() % (int)worldShape.numSectorX) - worldShape.numSectorX / 2;
    state.randomPos.pos.y = (rand() % (int)worldShape.numSectorY) - worldShape.numSectorY / 2;
}

TaskFunResult SectorPatrol::function(TaskFunArgs* args)
{
    auto* reg = args->ptrHandle->registry;
    auto* transform = reg->try_get<ecs::Transform>(args->entity);
    auto* sectorId = reg->try_get<ecs::SectorId>(args->entity);
    auto* moveCtrl = reg->try_get<ecs::MoveCtrl>(args->entity);
    if (!transform || !sectorId || !moveCtrl)
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        LG_D("SectorPatrol: EcsCompMissing");
        return TaskFunResult::EcsCompMissing;
    }
    if (!state.initialized)
    {
        makeRandomPos(args);
        state.initialized = true;
    }
    moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::MoveTo;
    moveCtrl->spPos = {.pos = {sectorId->x, sectorId->y},
                       .sectorPos = state.randomPos};
    moveCtrl->allowedPosError = config.allowedPosError;
    moveCtrl->allowedRotError = config.allowedRotError;
    moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::Forward;
    if (moveCtrl->moveMode != ecs::MoveCtrl::MoveMode::None
        && moveCtrl->posReached
        && moveCtrl->turnMode != ecs::MoveCtrl::TurnMode::None
        && moveCtrl->rotReached)
    {
        moveCtrl->posReached = false;
        moveCtrl->rotReached = false;
        moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::None;
        moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::None;
        makeRandomPos(args);
    }
    SCHED_NEXT(DEFAULT_INTERVAL);
    return TaskFunResult::Continue;
}

void SectorPatrol::makeRandomPos(TaskFunArgs* args)
{
    auto& worldShape = args->ptrHandle->world->getWorldShape();
    state.randomPos =
        0.9f
        * vec2(
            (rand() % (int)worldShape.sectorSize) - worldShape.sectorSize / 2,
            (rand() % (int)worldShape.sectorSize) - worldShape.sectorSize / 2);
}

TaskFunResult Patrol::function(TaskFunArgs* args)
{
    if (state.currentWayPointIndex >= config.wayPoints.size())
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        return TaskFunResult::Done;
    }
    auto* reg = args->ptrHandle->registry;
    auto* transform = reg->try_get<ecs::Transform>(args->entity);
    auto* sectorId = reg->try_get<ecs::SectorId>(args->entity);
    auto* moveCtrl = reg->try_get<ecs::MoveCtrl>(args->entity);
    if (!transform || !sectorId || !moveCtrl)
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        return TaskFunResult::EcsCompMissing;
    }
    auto& wayPoint = config.wayPoints[state.currentWayPointIndex];
    moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::MoveTo;
    moveCtrl->spPos = wayPoint;
    moveCtrl->allowedPosError = config.allowedPosError;
    moveCtrl->allowedRotError = config.allowedRotError;
    moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::Forward;
    if (moveCtrl->moveMode != ecs::MoveCtrl::MoveMode::None
        && moveCtrl->posReached
        && moveCtrl->turnMode != ecs::MoveCtrl::TurnMode::None
        && moveCtrl->rotReached)
    {
        moveCtrl->posReached = false;
        moveCtrl->rotReached = false;
        moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::None;
        moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::None;
        state.currentWayPointIndex++;
    }
    SCHED_NEXT(DEFAULT_INTERVAL);
    return TaskFunResult::Continue;
}

TaskFunResult Goto::function(TaskFunArgs* args)
{
    auto* reg = args->ptrHandle->registry;
    auto* transform = reg->try_get<ecs::Transform>(args->entity);
    auto* sectorId = reg->try_get<ecs::SectorId>(args->entity);
    auto* moveCtrl = reg->try_get<ecs::MoveCtrl>(args->entity);
    if (!transform || !sectorId || !moveCtrl)
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        return TaskFunResult::EcsCompMissing;
    }
    moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::MoveTo;
    moveCtrl->spPos = config.target;
    moveCtrl->allowedPosError = config.allowedPosError;
    moveCtrl->allowedRotError = config.allowedRotError;
    moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::Forward;
    if (moveCtrl->moveMode != ecs::MoveCtrl::MoveMode::None
        && moveCtrl->posReached
        && moveCtrl->turnMode != ecs::MoveCtrl::TurnMode::None
        && moveCtrl->rotReached)
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::None;
        moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::None;
        moveCtrl->posReached = false;
        moveCtrl->rotReached = false;
        return TaskFunResult::Done;
    }
    else
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        return TaskFunResult::Continue;
    }
}

TaskFunResult DebugLog::function(TaskFunArgs* args)
{
    long now = tim::nowU();
    LG_D("DebugLog: {} from entity {} at {} s",
         config.message,
         args->entityId,
         now / TIM_1S);
    SCHED_NEXT(DEFAULT_INTERVAL);
    return TaskFunResult::Done;
}

}  // namespace taskdata
}  // namespace ai