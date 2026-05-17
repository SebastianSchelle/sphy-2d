#include "comp-storage.hpp"
#include "comp-struct.hpp"
#include "ecs.hpp"
#include "lib-modules.hpp"
#include "mod-manager.hpp"

namespace ecs
{

void Storage::updateStatsFromEntity(entt::entity entity,
                                    ecs::PtrHandle* ptrHandle)
{
    auto& reg = ptrHandle->registry;
    auto* hull = reg->try_get<Hull>(entity);
    if (hull)
    {
        for (size_t i = 0; i < static_cast<size_t>(gobj::StorageType::NumStorageTypes); i++)
        {
            cargo[i].capacity = 0.0f;
            cargo[i].used = 0.0f;
        }
        for (const auto& module : hull->modules)
        {
            entt::entity moduleEntity =
                ptrHandle->ecs->getEntity(module.entityId);
            if (moduleEntity == entt::null)
            {
                continue;
            }
            if (module.slotType == gobj::ModuleType::Storage)
            {
                auto* moduleComp = reg->try_get<ecs::Module>(moduleEntity);
                auto* moduleData =
                    ptrHandle->modManager->getModuleLib().getItem(
                        moduleComp->moduleHandle);
                if (moduleData)
                {
                    auto& storage = std::get<gobj::mdata::Storage>(moduleData->data);
                    for (size_t i = 0; i < static_cast<size_t>(gobj::StorageType::NumStorageTypes); i++)
                    {
                        cargo[i].capacity += storage.volume[i];
                    }
                }
            }
        }
    }
}

}  // namespace ecs