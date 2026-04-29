#include "task-basic.hpp"
#include <ecs.hpp>

namespace ai
{
namespace taskdata
{

TaskFunResult Idle::function(TaskFunArgs* args)
{
    return TaskFunResult::Done;
}

TaskFunResult Patrol::function(TaskFunArgs* args)
{
    return TaskFunResult::Done;
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
    moveCtrl->active = true;
    moveCtrl->spPos = config.target;
    moveCtrl->faceDirMode = ecs::MoveCtrl::FaceDirMode::Forward;
    LG_D("Goto: target: {}, sector: {}, pos: {}, rot: {}",
         config.target.pos.x,
         config.target.pos.y,
         config.target.sectorPos.x,
         config.target.sectorPos.y);
    LG_D("Goto: moveCtrl.spPos: {}, moveCtrl.targetReached: {}", moveCtrl->spPos, moveCtrl->targetReached);
    if (moveCtrl->active && moveCtrl->targetReached)
    {
        LG_D("Goto: target reached");
        SCHED_NEXT(DEFAULT_INTERVAL);
        moveCtrl->active = false;
        moveCtrl->targetReached = false;
        return TaskFunResult::Done;
    }
    else
    {
        LG_D("Goto: target not reached yet");
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