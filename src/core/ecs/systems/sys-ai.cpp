#include <sys-ai.hpp>

namespace ecs
{
#ifdef SERVER
void sysAiImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    auto* reg = sector->getRegistry()->getRegistry();
    reg->view<EntityId, Ai>().each(
        [ptrHandle, sector](auto entity, auto& entityId, auto& ai)
        {
            if (ai.active)
            {
                if (ptrHandle->frameCnt < ai.nextRunFrame)
                {
                    // LG_D("sysAi: nextRunFrame: {}, frameCnt: {}",
                    //      ai->nextRunFrame,
                    //      ptrHandle->frameCnt);
                    return;
                }
                auto& taskSystem = sector->getTaskSystem();
                auto* taskStack = taskSystem.getTaskStack(ai.stackHandle);
                if (taskStack)
                {
                    ai::TaskFunArgs args = {
                        entityId, entity, ptrHandle, &ai.nextRunFrame, sector};
                    taskStack->runTask(&args);
                }
            }
        });
}
#endif
}  // namespace ecs