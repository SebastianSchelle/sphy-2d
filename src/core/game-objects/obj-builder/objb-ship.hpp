#ifndef OBJB_SHIP_HPP
#define OBJB_SHIP_HPP

#include "comp-phy.hpp"
#include "comp-storage.hpp"
#include "entt/entity/fwd.hpp"
#include <comp-struct.hpp>
#include <lib-hull.hpp>
#include <mod-manager.hpp>
#include <objb-general.hpp>

namespace objb
{

struct Hull
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::HullHandle& hullHandle)
    {
        gobj::Hull* hull =
            ptrHandle->modManager->getHullLib().getItem(hullHandle);
        OBJB_GUARD(hull,
                   "Hull: Could not find hull entry for {}",
                   hullHandle.toGenericHandle())
        params.reg.emplace_or_replace<ecs::Hull>(
            params.entity,
            ecs::Hull(hull->slots.size(),
                      hull->hullpoints,
                      hullHandle.toGenericHandle()));
        return true;
    }

    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::HullHandle& hullHandle,
                      const gobj::Hull& hull)
    {
        params.reg.emplace_or_replace<ecs::Hull>(
            params.entity,
            ecs::Hull(hull.slots.size(),
                      hull.hullpoints,
                      hullHandle.toGenericHandle()));
        return true;
    }
};

struct ShipHull
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::HullHandle& hullHandle)
    {
        OBJB_GUARD(Selectable::build(ptrHandle, params), "")
        gobj::Hull* hull =
            ptrHandle->modManager->getHullLib().getItem(hullHandle);
        OBJB_GUARD(
            Physics::build(
                ptrHandle,
                params,
                ecs::PhysicsBody{.mass = hull->mass > 0.0f ? hull->mass : 1.0f,
                                 .vel = vec2(0.0f, 0.0f),
                                 .acc = vec2(0.0f, 0.0f),
                                 .inertia = hull->inertia > 0.0f ? hull->inertia
                                                                 : 1.0f,
                                 .rotVel = 0.0f,
                                 .rotAcc = 0.0f}),
            "")
        OBJB_GUARD(hull,
                   "ShipHull: Could not find hull entry for {}",
                   hullHandle.toGenericHandle())
        OBJB_GUARD(Hull::build(ptrHandle, params, hullHandle, *hull),
                   "ShipHull: Failed to build ship hull")
        OBJB_GUARD(
            Collider::build(
                ptrHandle, params, hull->collider, ecs::CollisionLayer::Ship),
            "ShipHull: Failed to build collider")
        OBJB_GUARD(Textures::build(ptrHandle, params, hull->textures),
                   "ShipHull: Failed to build textures")
        OBJB_GUARD(Storage::build(ptrHandle, params, ecs::Storage{.cargo = {}}),
                   "")
        OBJB_GUARD(ThrusterMoveCtrl::build(
                       ptrHandle,
                       params,
                       ecs::PhyThrust{.maxTorque = 1000.0f,
                                      .maxRotVel = 3.0f,
                                      .thrustMainMax = 10000.0f,
                                      .thrustManeuverMax = 1000.0f,
                                      .maxSpd = 1000.0f},
                       ecs::MoveCtrl{.moveMode = ecs::MoveCtrl::MoveMode::None,
                                     .spPos = {},
                                     .allowedPosError = 100.0f,
                                     .turnMode = ecs::MoveCtrl::TurnMode::None,
                                     .allowedRotError = M_PIf}),
                   "")
        OBJB_GUARD(MapIcon::build(ptrHandle,
                                  params,
                                  IconShipHull{.sClass = hull->shipClass}),
                   "ShipHull: Failed to build map icon")
        return true;
    }

    static void updateStats(ecs::PtrHandle* ptrHandle,
        entt::registry *reg, entt::entity entity)
    {
        auto* phyThrust = reg->try_get<ecs::PhyThrust>(entity);
        if (phyThrust)
        {
            phyThrust->updateStatsFromEntity(entity, reg, ptrHandle);
        }
        auto* storage = reg->try_get<ecs::Storage>(entity);
        if (storage)
        {
            storage->updateStatsFromEntity(entity, reg, ptrHandle);
        }
    }

};

}  // namespace objb

#endif