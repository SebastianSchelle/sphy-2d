#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "ecs.hpp"
#include "lib-modules.hpp"
#include "mod-manager.hpp"

namespace ecs
{

void PhyThrust::updateStatsFromEntity(entt::entity entity,
                                      ecs::PtrHandle* ptrHandle)
{
    auto& reg = ptrHandle->registry;
    auto* hull = reg->try_get<Hull>(entity);
    if (hull)
    {
        thrustMainMax = 0.0f;
        thrustManeuverMax = 0.0f;
        for (const auto& module : hull->modules)
        {
            entt::entity moduleEntity =
                ptrHandle->ecs->getEntity(module.entityId);
            if (moduleEntity == entt::null)
            {
                continue;
            }
            if (module.slotType == gobj::ModuleType::MainThruster)
            {
                auto* moduleComp = reg->try_get<ecs::Module>(moduleEntity);
                auto* moduleData =
                    ptrHandle->modManager->getModuleLib().getItem(
                        moduleComp->moduleHandle);
                if (moduleData)
                {
                    thrustMainMax +=
                        std::get<gobj::mdata::MainThruster>(moduleData->data)
                            .maxThrust;
                }
            }
            else if (module.slotType == gobj::ModuleType::ManeuverThruster)
            {
                auto* moduleComp = reg->try_get<ecs::Module>(moduleEntity);
                auto* moduleData =
                    ptrHandle->modManager->getModuleLib().getItem(
                        moduleComp->moduleHandle);
                if (moduleData)
                {
                    thrustManeuverMax +=
                        std::get<gobj::mdata::ManeuverThruster>(
                            moduleData->data)
                            .maxThrust;
                }
            }
        }
    }
}

}  // namespace ecs
