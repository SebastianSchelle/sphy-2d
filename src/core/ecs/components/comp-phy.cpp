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

ContactEjectSpawn computeContactEjectSpawn(
    const Contact& contact,
    entt::entity surfaceEnt,
    entt::entity collisionEntFirst,
    vec2 sourceVel,
    const vec2* surfaceCenterFallback,
    const ContactEjectParams& params)
{
    // contact.normal is collisionEntFirst → other; flip so it points out of
    // surfaceEnt along the impact direction.
    vec2 ejectNormal = contact.normal;
    if (surfaceEnt != collisionEntFirst)
    {
        ejectNormal = -ejectNormal;
    }
    const float nLenSq = glm::dot(ejectNormal, ejectNormal);
    if (nLenSq > 1e-12f)
    {
        ejectNormal *= glm::inversesqrt(nLenSq);
    }
    else if (surfaceCenterFallback != nullptr)
    {
        ejectNormal = contact.point - *surfaceCenterFallback;
        const float fallbackLenSq = glm::dot(ejectNormal, ejectNormal);
        if (fallbackLenSq > 1e-12f)
        {
            ejectNormal *= glm::inversesqrt(fallbackLenSq);
        }
        else
        {
            ejectNormal = vec2(0.0f, 1.0f);
        }
    }

    ContactEjectSpawn out;
    out.pos = contact.point
              + ejectNormal * (contact.penetration + params.spawnClearance);
    out.vel = ejectNormal * params.baseEjectSpeed;
    const float along = std::max(0.0f, glm::dot(sourceVel, ejectNormal));
    out.vel += ejectNormal * along * params.sourceVelAlongFactor;
    if (params.computeRot)
    {
        out.rot = atan2f(-ejectNormal.x, ejectNormal.y);
    }
    return out;
}

}  // namespace ecs
