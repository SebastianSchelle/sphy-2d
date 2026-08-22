#include "comp-ident.hpp"
#include "logging.hpp"
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
    params.sector->objectInitBroadphase(ptr, slot->entity);
    ptr->engine->broadcastEntityToClients(shipHull);
    return shipHull;
}

ecs::EntityId AsteroidRecipe::spawn(const RecipeSpawnParams& params)
{
    auto ptr = params.ptrHandle;

    // Spawn asteroid
    auto asteroid = ptr->engine->spawnAsteroid(params.sector, asteroidHandle);

    // Place at desired pos and rotation
    auto slot = ptr->registryMapping->getEntity(asteroid);
    if (!slot)
    {
        LG_E("No registry slot found for entity {}", asteroid);
        return ecs::EntityId::Invalid();
    }
    auto reg = params.sector->getRegistry()->getRegistry();
    Transform::position(reg, slot->entity, params.pos, params.rot);
    Physics::naturalRot(reg, slot->entity, params.naturalRot);
    params.sector->objectInitBroadphase(ptr, slot->entity);
    ptr->engine->broadcastEntityToClients(asteroid);
    return asteroid;
}

}  // namespace objb
