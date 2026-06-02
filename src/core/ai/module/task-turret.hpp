#ifndef TASK_TURRET_HPP
#define TASK_TURRET_HPP

#include <task.hpp>

namespace ecs
{
struct Transform;
struct Turret;
}

namespace gobj
{
namespace mdata
{
struct Turret;
}
}

namespace ai
{
namespace taskdata
{

struct Turret
{
    enum class Mode : uint8_t
    {
        None,
        Player,
        Mine,
        NumModes,
    };

    struct BestTarget
    {
        ecs::EntityId entityId;
        float score;
    };

    TASK_HEADER("turret", 60);

    struct ScoreArgs
    {
        entt::entity tgt;
        float maxRange;
        ecs::Transform& trSelf;
        ecs::Turret& turret;
        gobj::mdata::Turret& turretData;
    };
    using ScoreFunction = std::function<float(ScoreArgs& scoreArgs)>;
    bool findBestTarget(TaskFunArgs* args,
                        BestTarget& bestTarget,
                        ScoreFunction scoreFunction);
    TaskFunResult function(TaskFunArgs* args);
    TaskFunResult funNone(TaskFunArgs* args);
    TaskFunResult funPlayer(TaskFunArgs* args);
    TaskFunResult funMine(TaskFunArgs* args);

    struct ConfigNone
    {
    };
    struct ConfigPlayer
    {
    };
    struct ConfigMine
    {
    };
    using ConfigData = std::variant<ConfigNone, ConfigPlayer, ConfigMine>;
    struct Config
    {
        Mode mode = Mode::None;
        ConfigData configData;
    };
    Config config = Config{
        Mode::None,
        ConfigNone{},
    };

    struct StateNone
    {
    };
    struct StatePlayer
    {
    };
    struct StateMine
    {
    };
    using StateData = std::variant<StateNone, StatePlayer, StateMine>;
    struct State
    {
        StateData stateData;
    };
    State state = State{
        StateNone{},
    };
};

}  // namespace taskdata
}  // namespace ai

#endif