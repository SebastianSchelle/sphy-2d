#ifndef TASK_SYSTEM_HPP
#define TASK_SYSTEM_HPP

#include <task-data.hpp>
#include <free-vector.hpp>
#include <ecs.hpp>
#include <ptr-handle.hpp>

namespace ai
{

enum class TaskFunResult
{
    Done,
    Failed,
    Continue,
};

typedef uint16_t taskId;

struct TaskFunArgs
{
  ecs::EntityId entityId;
  entt::entity entity;
  ecs::PtrHandle* ptrHandle;
};

typedef TaskFunResult(*TaskFunction)(TaskFunArgs* args);

class Task
{
  public:
    Task() = default;
    ~Task() = default;

  private:
    taskId id;
    taskdata::TaskData data;
};

class TaskStack
{
  public:
    TaskStack() = default;
    ~TaskStack() = default;

  private:
    Task defaultTask;
    std::vector<Task> tasks;
};
using TaskStackHandle = typename con::FreeVec<TaskStack>::Handle;

class TaskSystem
{
  public:
    TaskSystem() = default;
    ~TaskSystem() = default;

  private:
    con::FreeVec<TaskStack> taskStacks;
};


}  // namespace ai

#endif
