#ifndef TASK_SYSTEM_HPP
#define TASK_SYSTEM_HPP

#include <free-vector.hpp>
#include <ptr-handle.hpp>
#include <std-inc.hpp>
#include <task-basic.hpp>
#include <task.hpp>

namespace ai
{

namespace taskdata
{
using TaskData = std::variant<Idle, Patrol, Goto, DebugLog>;
}  // namespace taskdata

class TaskStack
{
  public:
    TaskStack(const taskdata::TaskData& defaultTask = taskdata::Idle());
    ~TaskStack();

    TaskFunResult runTask(TaskFunArgs* args);
    void addTask(const taskdata::TaskData& task);
    void setDefaultTask(const taskdata::TaskData& defaultTask);

  private:
    taskdata::TaskData defaultTask;
    vector<taskdata::TaskData> tasks;
};
using TaskStackHandle = typename con::FreeVec<TaskStack>::Handle;

class TaskSystem
{
  public:
    TaskSystem();
    ~TaskSystem();

    TaskStackHandle
    createTaskStack(const taskdata::TaskData& defaultTask = taskdata::Idle());

    TaskStack* getTaskStack(TaskStackHandle handle);
    TaskFunResult runTask(TaskStackHandle stackHandle, TaskFunArgs* args);
    void addTask(TaskStackHandle stackHandle, const taskdata::TaskData& task);

  private:
    con::FreeVec<TaskStack> taskStacks;
};


}  // namespace ai

#endif
