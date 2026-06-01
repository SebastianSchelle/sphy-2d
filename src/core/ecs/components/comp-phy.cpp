#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "ecs.hpp"
#include "lib-modules.hpp"
#include "mod-manager.hpp"

namespace ecs
{

const CollisionLayerMat::Interaction&
CollisionLayerMat::getInteraction(CollisionLayer layer1,
                                  CollisionLayer layer2) const
{
    return interactions[static_cast<size_t>(layer1)]
                       [static_cast<size_t>(layer2)];
}

void CollisionLayerMat::setInteraction(CollisionLayer layer1,
                                       CollisionLayer layer2,
                                       Interaction interaction)
{
    interactions[static_cast<size_t>(layer1)][static_cast<size_t>(layer2)] =
        interaction;
    interactions[static_cast<size_t>(layer2)][static_cast<size_t>(layer1)] =
        interaction;
    LG_I("Set collision between {} and {} to {}",
         magic_enum::enum_name(layer1),
         magic_enum::enum_name(layer2),
         interaction);
}


void PhyThrust::updateStatsFromEntity(entt::entity entity,
                                      ecs::PtrHandle* ptrHandle)
{
    auto& reg = ptrHandle->registry;
    auto* hull = reg->try_get<Hull>(entity);
    if (hull)
    {
        auto hullData =
            ptrHandle->modManager->getHullLib().getItem(hull->hullHandle);
        if (hullData)
        {
            maxTorque = hullData->internalGyroTorque;
        }
        else
        {
            LG_E("Hull data not found for entity: {}", entity);
            maxTorque = 100000.0f;
        }
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
    else
    {
        LG_E("Hull not found for entity: {}", entity);
        maxTorque = 0.0f;
        thrustMainMax = 0.0f;
        thrustManeuverMax = 0.0f;
    }
}


}  // namespace ecs
