#ifndef OBJB_MODULE_HPP
#define OBJB_MODULE_HPP

#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "comp-struct.hpp"
#include "lib-modules.hpp"
#include <comp-turret.hpp>
#include <mod-manager.hpp>
#include <objb-general.hpp>
#include <world.hpp>

namespace objb
{
namespace module
{

struct Turret
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::mdata::Turret& data,
                      ecs::EntityId exceptEntId)
    {
        params.reg.emplace_or_replace<ecs::Turret>(
            params.entity,
            ecs::Turret{
                .aimMode = ecs::Turret::AimMode::None,
                .aimData = ecs::Turret::AngleData{0.0f},
                .data = ecs::Turret::fromGobjTurretData(data, exceptEntId),
                .currentAngle = 0.0f,
                .isFiring = false,
            });
        return true;
    }
};

struct Module
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      ecs::EntityId parent,
                      const gobj::ModuleHandle& moduleHandle,
                      uint16_t slotIndex)
    {
        auto slot = ptrHandle->registryMapping->getEntity(parent);
        OBJB_GUARD(slot, "Could not spawn module. Invalid parent entityId")
        auto sector = ptrHandle->world->getSector(slot->sectorId);
        OBJB_GUARD(sector, "Could not spawn module. Sector not found")
        auto module =
            ptrHandle->modManager->getModuleLib().getItem(moduleHandle);
        OBJB_GUARD(module, "Could not spawn module. Module info not found")
        auto reg = sector->getRegistry()->getRegistry();
        auto hull = reg->try_get<ecs::Hull>(slot->entity);
        OBJB_GUARD(hull, "Could not spawn module. Parent has no hull")
        auto hullItem =
            ptrHandle->modManager->getHullLib().getItem(hull->hullHandle);
        OBJB_GUARD(hullItem,
                   "Could not spawn module. Parent hull info not found")
        OBJB_GUARD(slotIndex < hullItem->slots.size(),
                   "Could not spawn module. Invalid slot index")
        auto hullSlot = hullItem->slots[slotIndex];
        OBJB_GUARD(hullSlot.type == module->slotType,
                   "Could not spawn module. Incompatible slot type")
        OBJB_GUARD(Textures::build(ptrHandle, params, module->textures),
                   "Failed to build module texture")
        OBJB_GUARD(Transform::build(ptrHandle, params), "");
        OBJB_GUARD(AnchorFixed::build(ptrHandle,
                                      params,
                                      ecs::AnchorFixed{.pos = hullSlot.pos,
                                                       .rot = hullSlot.rot,
                                                       .ref = parent}),
                   "Failed to build module anchor")
        reg->emplace_or_replace<ecs::Module>(
            params.entity,
            ecs::Module{moduleHandle.toGenericHandle(),
                        parent.toGenericHandle32()});

        switch (module->type)
        {
            case gobj::ModuleType::MainThruster:
                break;
            case gobj::ModuleType::ManeuverThruster:
                break;
            case gobj::ModuleType::Storage:
                break;
            case gobj::ModuleType::Turret:
            {
                OBJB_GUARD(
                    Turret::build(ptrHandle,
                                  params,
                                  std::get<gobj::mdata::Turret>(module->data),
                                  parent),
                    "");
                OBJB_GUARD(Ai::build(ptrHandle,
                                     params,
                                     ai::taskdata::Turret{
                                         ai::taskdata::Turret::Mode::Mine,
                                         ai::taskdata::Turret::ConfigMine{}}),
                           "Failed to add Ai to turret module")
                break;
            }
            default:
                break;
        }
        hull->addModule(
            slotIndex,
            ecs::ModuleRef{params.entityId, module->type, module->slotType});
        return true;
    }
};

}  // namespace module
}  // namespace objb

#endif
