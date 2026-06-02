#ifndef SYS_AI_HPP
#define SYS_AI_HPP

#include <components/comp-ai.hpp>
#include <ecs.hpp>
#include <task-system.hpp>
#include <sector.hpp>

namespace ecs
{

const System sysAi = {
    .name = "sysAi",
    .type = SystemType::SectorForeachEntitiy,
    .function = SFSectorForeach{
        [](world::Sector* sector,
           const entt::entity entity,
           const ecs::EntityId& entityId,
           const float dt,
           PtrHandle* ptrHandle)
        {
            auto reg = ptrHandle->registry;
            auto* ai = reg->try_get<Ai>(entity);
            if (ai && ai->active)
            {
                if (ptrHandle->frameCnt < ai->nextRunFrame)
                {
                    // LG_D("sysAi: nextRunFrame: {}, frameCnt: {}",
                    //      ai->nextRunFrame,
                    //      ptrHandle->frameCnt);
                    return;
                }
                auto& taskSystem = sector->getTaskSystem();
                auto* taskStack = taskSystem.getTaskStack(ai->stackHandle);
                if (taskStack)
                {
                    ai::TaskFunArgs args = {entityId,
                                            entity,
                                            ptrHandle,
                                            &ai->nextRunFrame,
                                            sector};
                    taskStack->runTask(&args);
                }
            }
        }}};

}  // namespace ecs

#endif