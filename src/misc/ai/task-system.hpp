#ifndef TASK_SYSTEM_HPP
#define TASK_SYSTEM_HPP

#include <item-lib.hpp>
#include <task-data.hpp>

namespace ai
{

enum class TaskFunResult
{
    Done,
    Failed,
    Continue,
};

typedef uint16_t taskId;
typedef std::function<TaskFunResult(void)> TaskFunction;

class Task
{
  public:
    Task();
    ~Task();

  private:
    taskId id;
    taskdata::TaskData data;
};

class TaskStack
{
  public:
    TaskStack();
    ~TaskStack();

  private:
    Task defaultTask;
    std::vector<Task> tasks;
};
using TaskStackHandle = typename con::ItemLib<TaskStack>::Handle;

class TaskSystem
{
  public:
    TaskSystem();
    ~TaskSystem();

  private:
    con::ItemLib<TaskStack> taskStacks;
};


}  // namespace ai

#endif
