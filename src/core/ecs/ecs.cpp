#include "ecs.hpp"

namespace ecs
{
Ecs::Ecs() {}

Ecs::~Ecs() {}

EntityId Ecs::createEntity()
{
    entt::entity e = registry.create();

    uint32_t index;
    if (!idMapFreeSlots.empty())
    {
        index = idMapFreeSlots.back();
        idMapFreeSlots.pop_back();
    }
    else
    {
        index = idMap.size();
        idMap.push_back({});
    }
    Slot& slot = idMap[index];
    EntityId entityId = {index, slot.generation};
    registry.emplace<EntityId>(e, entityId);

    return entityId;
}

void Ecs::destroyEntity(EntityId entityId)
{
    if (!validId(entityId))
    {
        return;
    }
    Slot& slot = idMap[entityId.index];
    registry.destroy(slot.entity);
    slot.generation++;
    slot.entity = entt::null;
    idMapFreeSlots.push_back(entityId.index);
}

bool Ecs::validId(EntityId entityId)
{
    return entityId.index < idMap.size()
           && idMap[entityId.index].generation == entityId.generation
           && idMap[entityId.index].entity != entt::null;
}

entt::entity Ecs::getEntity(EntityId entityId)
{
    if (!validId(entityId))
    {
        return entt::null;
    }
    return idMap[entityId.index].entity;
}

const vector<System>& Ecs::getRegisteredSystems()
{
    return registeredSystems;
}

void Ecs::registerSystem(const System system)
{
    if (std::find(registeredSystems.begin(), registeredSystems.end(), system)
        != registeredSystems.end())
    {
        return;
    }
    registeredSystems.push_back(system);
    LG_D("Registered ECS system: {}", system.name);
}

bool Ecs::spawnEntityFromAsset(EntityId entityId, const std::string& assetId, const AssetFactory& assetFactory)
{
    entt::entity entity = getEntity(entityId);
    if (entity == entt::null)
    {
        LG_E("Entity not found: {}", entityId);
        return false;
    }
    assetFactory.copyComponentsIntoEntity(registry, entity, assetId);
    return true;
}

entt::registry& Ecs::getRegistry()
{
    return registry;
}

}  // namespace ecs