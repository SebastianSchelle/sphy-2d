#ifndef TASK_HPP
#define TASK_HPP

#include <comp-ident.hpp>
#include <entt/entt.hpp>
#include <ptr-handle.hpp>
#include <world-def.hpp>

#define TASK_HEADER(name, defaultInterval)                                     \
    static constexpr string NAME = name;                                       \
    static constexpr uint32_t ID = hashConst(NAME.c_str());                    \
    static constexpr uint32_t DEFAULT_INTERVAL = defaultInterval;
#define SCHED_NEXT(interval) *args->nextRunFrame = args->ptrHandle->frameCnt + (interval);

namespace ai
{

enum class TaskFunResult
{
    Done,
    Failed,
    Continue,
    NoStack,
    NoTask,
    EcsCompMissing,
};

struct TaskFunArgs
{
    ecs::EntityId entityId;
    entt::entity entity;
    ecs::PtrHandle* ptrHandle;
    uint32_t* nextRunFrame;
};

typedef TaskFunResult (*TaskFunction)(TaskFunArgs* args);

}  // namespace ai

#endif