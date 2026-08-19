#include "comp-ident.hpp"
#include "logging.hpp"
#include "objb-general.hpp"
#include "objb-ship.hpp"
#include "objb-item.hpp"
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

ecs::EntityId ProjectileRecipe::spawn(const RecipeSpawnParams& params,
                                      float s,
                                      float c)
{
    auto ptr = params.ptrHandle;

    // Spawn asteroid
    auto projectile = ptr->engine->spawnProjectile(
        params.sector, projectileHandle, exceptEntity);

    // Place at desired pos and rotation
    auto slot = ptr->registryMapping->getEntity(projectile);
    if (!slot)
    {
        LG_E("No registry slot found for entity {}", projectile);
        return ecs::EntityId::Invalid();
    }
    auto reg = params.sector->getRegistry()->getRegistry();
    Transform::position(reg, slot->entity, params.pos, params.rot);

    // calculate exit velocity
    const vec2 fireDir = smath::rotateVec2(vec2(0.0f, 1.0f), s, c);
    vec2 fireVel = fireDir * exitSpeed;
    Physics::velocity(reg, slot->entity, params.vel + fireVel);
    params.sector->objectInitBroadphase(ptr, slot->entity);
    ptr->engine->broadcastEntityToClients(projectile);
    return projectile;
}

ecs::EntityId ItemRecipe::spawn(const RecipeSpawnParams& params, float quantity)
{
    auto ptr = params.ptrHandle;

    // Spawn asteroid
    auto item = ptr->engine->spawnItem(
        params.sector, itemHandle, collExcept);

    // Place at desired pos and rotation
    auto slot = ptr->registryMapping->getEntity(item);
    if (!slot)
    {
        LG_E("No registry slot found for entity {}", item);
        return ecs::EntityId::Invalid();
    }
    auto reg = params.sector->getRegistry()->getRegistry();
    Transform::position(reg, slot->entity, params.pos, params.rot);

    // Set amount
    Item::quantity(reg, slot->entity, quantity);

    // Set velocity of item
    Physics::velocity(reg, slot->entity, params.vel);
    params.sector->objectInitBroadphase(ptr, slot->entity);
    ptr->engine->broadcastEntityToClients(item);
    return item;
}

}  // namespace objb
