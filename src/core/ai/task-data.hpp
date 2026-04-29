#ifndef TASK_DATA_HPP
#define TASK_DATA_HPP

#include <std-inc.hpp>
#include <world-def.hpp>

namespace ai
{
namespace taskdata
{

struct Patrol
{
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
    struct Config
    {
        def::SectorCoords target;
    };
    Config config;
};

struct DebugLog
{
    struct Config
    {
        std::string message;
    };
    Config config;
};

using TaskData = std::variant<Patrol, Goto, DebugLog>;

}  // namespace taskdata

}  // namespace ai

#endif
