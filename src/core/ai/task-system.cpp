#include <task-system.hpp>
#include <memory>
#include <magic_enum/magic_enum.hpp>

namespace ai
{

constexpr uint32_t IDLE_TASK_INTERVAL = 60;

TaskStack::TaskStack(const taskdata::TaskData& defaultTask)
    : defaultTask(defaultTask)
{
}

TaskStack::~TaskStack() {}

void TaskStack::setDefaultTask(const taskdata::TaskData& defaultTask)
{
    taskdata::TaskData tmp(defaultTask);
    std::destroy_at(&this->defaultTask);
    std::construct_at(&this->defaultTask, std::move(tmp));
}

void TaskStack::addTaskFirst(const taskdata::TaskData& task)
{
    tasks.push_back(task);
}

void TaskStack::addTaskLast(const taskdata::TaskData& task)
{
    tasks.insert(tasks.begin(), task);
}

void TaskStack::addTaskReplaceAll(const taskdata::TaskData& task)
{
    tasks.clear();
    tasks.push_back(task);
}

TaskFunResult TaskStack::runTask(TaskFunArgs* args)
{
    if (tasks.empty())
    {
        if (std::holds_alternative<taskdata::Idle>(defaultTask))
        {
            *args->nextRunFrame =
                args->ptrHandle->frameCnt + IDLE_TASK_INTERVAL;
            return TaskFunResult::NoTask;
        }
        else
        {
            tasks.push_back(defaultTask);
        }
    }
    auto& task = tasks.back();
    auto result = std::visit(
        [args](auto& taskData) { return taskData.function(args); }, task);
    if (result != TaskFunResult::Continue)
    {
        tasks.pop_back();
    }
    return result;
}

TaskSystem::TaskSystem() {}

TaskSystem::~TaskSystem() {}

TaskStackHandle
TaskSystem::createTaskStack(const taskdata::TaskData& defaultTask)
{
    TaskStack stack(defaultTask);
    return taskStacks.addItem(stack);
}

TaskStack* TaskSystem::getTaskStack(TaskStackHandle handle)
{
    return taskStacks.getItem(handle);
}

TaskFunResult TaskSystem::runTask(TaskStackHandle stackHandle,
                                  TaskFunArgs* args)
{
    auto* taskStack = getTaskStack(stackHandle);
    if (!taskStack)
    {
        return TaskFunResult::NoStack;
    }
    return taskStack->runTask(args);
}

void TaskSystem::addTaskFirst(TaskStackHandle stackHandle,
                         const taskdata::TaskData& task)
{
    auto* taskStack = getTaskStack(stackHandle);
    if (!taskStack)
    {
        LG_E("No task stack for handle: {}", stackHandle.toGenericHandle());
        return;
    }
    taskStack->addTaskFirst(task);
}

void TaskSystem::addTaskLast(TaskStackHandle stackHandle,
                             const taskdata::TaskData& task)
{
    auto* taskStack = getTaskStack(stackHandle);
    if (!taskStack)
    {
        LG_E("No task stack for handle: {}", stackHandle.toGenericHandle());
        return;
    }
    taskStack->addTaskLast(task);
}

void TaskSystem::addTaskReplaceAll(TaskStackHandle stackHandle,
                                   const taskdata::TaskData& task)
{
    auto* taskStack = getTaskStack(stackHandle);
    if (!taskStack)
    {
        LG_E("No task stack for handle: {}", stackHandle.toGenericHandle());
        return;
    }
    taskStack->addTaskReplaceAll(task);
}

}  // namespace ai
