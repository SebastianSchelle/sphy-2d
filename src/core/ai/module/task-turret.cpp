#include "task-turret.hpp"
#include <comp-struct.hpp>
#include <comp-turret.hpp>
#include <ecs.hpp>
#include <mod-manager.hpp>
#include <sector.hpp>

namespace ai
{
namespace taskdata
{

TaskFunResult Turret::function(TaskFunArgs* args)
{
    switch (config.mode)
    {
        case Mode::None:
            return funNone(args);
        case Mode::Player:
            return funPlayer(args);
        case Mode::Mine:
            return funMine(args);
        default:
            return TaskFunResult::Failed;
    }
}

bool Turret::findBestTarget(TaskFunArgs* args,
                            BestTarget& bestTarget,
                            ScoreFunction scoreFunction)
{
    if (!args->sector)
    {
        return false;
    }
    auto* reg = args->ptrHandle->registry;
    auto* transform = reg->try_get<ecs::Transform>(args->entity);
    auto* turret = reg->try_get<ecs::Turret>(args->entity);
    auto* module = reg->try_get<ecs::Module>(args->entity);
    if (!transform || !turret || !module)
    {
        return false;
    }
    auto* moduleData = args->ptrHandle->modManager->getModuleLib().getItem(
        module->moduleHandle);
    if (!moduleData)
    {
        return false;
    }
    auto& turretData = std::get<gobj::mdata::Turret>(moduleData->data);
    float range = turretData.range;

    bool hit = false;
    bestTarget.score = std::numeric_limits<float>::max();
    args->sector->queryBroadphase(
        {transform->pos - range, transform->pos + range},
        [&, this](entt::entity entity)
        {
            // todo: don't use transform pos, use nearest collision point
            ScoreArgs scoreArgs = {
                entity,
                range,
                *transform,
                *turret,
                turretData,
            };
            float score = scoreFunction(scoreArgs);
            if (score > 0.0f)
            {
                if (score < bestTarget.score)
                {
                    bestTarget.entityId = reg->get<ecs::EntityId>(entity);
                    bestTarget.score = score;
                }
                hit = true;
            }
        });
    return hit;
}

TaskFunResult Turret::funNone(TaskFunArgs* args)
{
    return TaskFunResult::Done;
}

TaskFunResult Turret::funMine(TaskFunArgs* args)
{
    BestTarget bestTarget;
    ScoreFunction scoreFunction = [args](ScoreArgs& scoreArgs) -> float
    {
        auto* reg = args->ptrHandle->registry;
        auto* asteroid = reg->try_get<ecs::Asteroid>(scoreArgs.tgt);
        if (!asteroid)
        {
            return -1.0f;
        }
        auto* trTarget = reg->try_get<ecs::Transform>(scoreArgs.tgt);
        if (!trTarget)
        {
            return -1.0f;
        }
        float distance = glm::distance(scoreArgs.trSelf.pos, trTarget->pos);
        if (distance > scoreArgs.maxRange)
        {
            return -1.0f;
        }
        return distance;
    };
    if (findBestTarget(args, bestTarget, scoreFunction))
    {
        auto* reg = args->ptrHandle->registry;
        auto* turret = reg->try_get<ecs::Turret>(args->entity);
        if (!turret)
        {
            LG_E("No turret component");
            return TaskFunResult::Failed;
        }
        LG_D("Found best target: {} with score {}",
             bestTarget.entityId,
             bestTarget.score);
        turret->fireMode = ecs::Turret::FireMode::AutoAngle;
        turret->aimMode = ecs::Turret::AimMode::Entity;
        turret->aimData = ecs::Turret::EntityData{bestTarget.entityId};

        SCHED_NEXT(DEFAULT_INTERVAL);
        return TaskFunResult::Continue;
    }
    else
    {
        auto* reg = args->ptrHandle->registry;
        auto* turret = reg->try_get<ecs::Turret>(args->entity);
        if (!turret)
        {
            LG_E("No turret component");
            return TaskFunResult::Failed;
        }
        turret->setAimMode(ecs::Turret::AimMode::None);
        turret->fireMode = ecs::Turret::FireMode::None;
        SCHED_NEXT(DEFAULT_INTERVAL);
        return TaskFunResult::Continue;
    }
}

TaskFunResult Turret::funPlayer(TaskFunArgs* args)
{
    auto* reg = args->ptrHandle->registry;
    auto* turret = reg->try_get<ecs::Turret>(args->entity);
    if (!turret)
    {
        LG_E("No turret component");
        return TaskFunResult::Failed;
    }
    turret->setAimMode(ecs::Turret::AimMode::Player);
    SCHED_NEXT(DEFAULT_INTERVAL * 10);
    return TaskFunResult::Done;
}

}  // namespace taskdata
}  // namespace ai
