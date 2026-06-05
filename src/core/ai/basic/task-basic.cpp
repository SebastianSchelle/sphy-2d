#include "task-basic.hpp"
#include <ecs.hpp>
#include <world.hpp>

namespace ai
{
namespace taskdata
{

bool applyMoveToTarget(ecs::MoveCtrl* moveCtrl, const MoveToTarget& target)
{
    moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::MoveTo;
    moveCtrl->spPos = target.spPos;
    moveCtrl->allowedPosError = target.allowedPosError;
    moveCtrl->allowedRotError = target.allowedRotError;
    moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::Forward;
    moveCtrl->faceDirData =
        ecs::MoveCtrl::MCForwardData{target.minFaceForwardDist};

    if (moveCtrl->moveMode != ecs::MoveCtrl::MoveMode::Brake
        && moveCtrl->posReached
        && moveCtrl->turnMode != ecs::MoveCtrl::TurnMode::Brake
        && moveCtrl->rotReached)
    {
        moveCtrl->posReached = false;
        moveCtrl->rotReached = false;
        moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::Brake;
        moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::Brake;
        return true;
    }
    return false;
}

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
    if (applyMoveToTarget(moveCtrl,
                          {.spPos = state.randomPos,
                           .allowedPosError = config.allowedPosError,
                           .allowedRotError = config.allowedRotError}))
    {
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
    state.randomPos.pos.x = (rand() % (int)worldShape.numSectorX);
    state.randomPos.pos.y = (rand() % (int)worldShape.numSectorY);
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
    if (applyMoveToTarget(
            moveCtrl,
            {.spPos = {.pos = {sectorId->x, sectorId->y},
                       .sectorPos = state.randomPos},
             .allowedPosError = config.allowedPosError,
             .allowedRotError = config.allowedRotError}))
    {
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
    if (applyMoveToTarget(moveCtrl,
                          {.spPos = wayPoint,
                           .allowedPosError = config.allowedPosError,
                           .allowedRotError = config.allowedRotError}))
    {
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
    if (applyMoveToTarget(moveCtrl,
                          {.spPos = config.target,
                           .allowedPosError = config.allowedPosError,
                           .allowedRotError = config.allowedRotError}))
    {
        SCHED_NEXT(DEFAULT_INTERVAL);
        return TaskFunResult::Done;
    }
    SCHED_NEXT(DEFAULT_INTERVAL);
    return TaskFunResult::Continue;
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