#ifndef TASK_DATA_HPP
#define TASK_DATA_HPP

#include <cmath>
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

struct SectorPatrol
{
    TASK_HEADER("sector-patrol", 600);
    TaskFunResult function(TaskFunArgs* args);
    void makeRandomPos(TaskFunArgs* args);

    struct Config
    {
        float allowedPosError = 100.0f;
        float allowedRotError = M_PIf;
    };
    struct State
    {
        vec2 randomPos = {0.0f, 0.0f};
        bool initialized = false;
    };
    Config config;
    State state;
};

struct Patrol
{
    TASK_HEADER("patrol", 600);
    TaskFunResult function(TaskFunArgs* args);

    struct Config
    {
        std::vector<def::SectorCoords> wayPoints;
        float allowedPosError = 10.0f;
        float allowedRotError = M_PIf;
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
        float allowedPosError = 10.0f;
        float allowedRotError = M_PIf;
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
