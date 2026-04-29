#ifndef TASK_DATA_HPP
#define TASK_DATA_HPP

#include <std-inc.hpp>
#include <world-def.hpp>
#include <task.hpp>

namespace ai
{
namespace taskdata
{

struct Idle
{
    TASK_HEADER("idle", 60);
    TaskFunResult function(TaskFunArgs* args);
};

struct Patrol
{
    TASK_HEADER("patrol", 60);
    TaskFunResult function(TaskFunArgs* args);

    struct Config
    {
        std::vector<def::SectorCoords> wayPoints;
    };
    struct State
    {
        uint16_t currentWayPointIndex = 0;
    };
    Config config;
    State state;
};

struct Goto
{
    TASK_HEADER("goto", 60);
    TaskFunResult function(TaskFunArgs* args);

    struct Config
    {
        def::SectorCoords target;
    };
    Config config;
};

struct DebugLog
{
    TASK_HEADER("debug-log", 60);
    TaskFunResult function(TaskFunArgs* args);

    struct Config
    {
        std::string message;
    };
    Config config;
};

}  // namespace taskdata

}  // namespace ai

#endif
