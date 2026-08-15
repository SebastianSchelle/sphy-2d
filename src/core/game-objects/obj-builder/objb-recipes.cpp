#include "comp-ident.hpp"
#include "objb-general.hpp"
#include "objb-ship.hpp"
#include "ptr-handle.hpp"
#include <engine.hpp>
#include <mod-manager.hpp>
#include <objb-recipes.hpp>
namespace objb
{

ecs::EntityId ShipRecipe::spawn(const RecipeSpawnParams& params)
{
    auto ptr = params.ptrHandle;

    // Spawn ship + modules
    auto shipHull = ptr->engine->spawnShipHull(params.sector, hullHandle);
    for (auto ms : modSlot)
    {
        auto mod = ptr->engine->spawnModule(
            params.sector, shipHull, ms.modHandle, ms.slot);
    }

    // Update ship stats from modules
    auto slot = ptr->registryMapping->getEntity(shipHull);
    if (!slot)
    {
        LG_E("No registry slot found for entity {}", shipHull);
        return ecs::EntityId::Invalid();
    }
    auto reg = params.sector->getRegistry()->getRegistry();
    ShipHull::updateStats(params.ptrHandle, reg, slot->entity);

    // Place at desired pos
    Transform::position(reg, slot->entity, params.pos, params.rot);
    return shipHull;
}

}  // namespace objb
