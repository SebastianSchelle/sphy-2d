#include "comp-storage.hpp"
#include "comp-struct.hpp"
#include "ecs.hpp"
#include "lib-item.hpp"
#include "lib-modules.hpp"
#include "mod-manager.hpp"
#include "registry-mapping.hpp"

namespace ecs
{

void Storage::updateStatsFromEntity(entt::entity entity,
                                    entt::registry* reg,
                                    ecs::PtrHandle* ptrHandle)
{
    auto* hull = reg->try_get<Hull>(entity);
    if (hull)
    {
        auto hullData =
            ptrHandle->modManager->getHullLib().getItem(hull->hullHandle);
        if (hullData)
        {
            for (size_t i = 0; i < static_cast<size_t>(
                                   gobj::ItemStorageType::NumStorageTypes);
                 i++)
            {
                cargo[i].capacity = hullData->volume[i];
            }
        }
        else
        {
            LG_E("Hull data not found for entity: {}", (uint32_t)entity);
            for (size_t i = 0; i < static_cast<size_t>(
                                   gobj::ItemStorageType::NumStorageTypes);
                 i++)
            {
                cargo[i].capacity = 0.0f;
            }
        }
        for (const auto& module : hull->modules)
        {
            auto slot = ptrHandle->registryMapping->getEntity(module.entityId);
            if (!slot)
            {
                continue;
            }
            if (module.slotType == gobj::ModuleType::Storage)
            {
                auto* moduleComp = reg->try_get<ecs::Module>(slot->entity);
                auto* moduleData =
                    ptrHandle->modManager->getModuleLib().getItem(
                        moduleComp->moduleHandle);
                if (moduleData)
                {
                    auto& storage =
                        std::get<gobj::mdata::Storage>(moduleData->data);
                    for (size_t i = 0;
                         i < static_cast<size_t>(
                             gobj::ItemStorageType::NumStorageTypes);
                         i++)
                    {
                        cargo[i].capacity += storage.volume[i];
                    }
                }
            }
        }
    }
}

uint32_t Storage::tryAddItem(const gobj::ItemHandle& itemHandle,
                             const gobj::Item& item,
                             uint32_t quantity)
{
    const gobj::ItemStorageType storageType = item.storageType;
    if (storageType >= gobj::ItemStorageType::NumStorageTypes)
    {
        LG_E("Invalid storage type: {}", storageType);
        return false;
    }
    uint32_t spaceLeft = (cargo[static_cast<size_t>(storageType)].capacity
                          - cargo[static_cast<size_t>(storageType)].used)
                         / item.volume;
    uint32_t quantityToAdd = std::min(spaceLeft, quantity);
    if (quantityToAdd <= 0)
    {
        return 0;
    }

    auto it = std::find_if(slots.begin(),
                           slots.end(),
                           [itemHandle](const StorageSlot& slot)
                           {
                               return gobj::ItemHandle(slot.itemHandle).value()
                                      == itemHandle.value();
                           });
    if (it != slots.end())
    {
        it->quantity += quantityToAdd;
    }
    else
    {
        slots.push_back(
            StorageSlot{itemHandle.toGenericHandle(), quantityToAdd});
    }
    cargo[static_cast<size_t>(storageType)].used += quantityToAdd * item.volume;
    return quantityToAdd;
}

}  // namespace ecs