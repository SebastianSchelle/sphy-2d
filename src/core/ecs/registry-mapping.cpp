#include "registry-mapping.hpp"

namespace ecs
{

RegistryMapping::RegistryMapping()
{
}

RegistryMapping::~RegistryMapping()
{
}

EntityId RegistryMapping::registerEntity(uint32_t sectorId, entt::entity entity)
{
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
    EntMapSlot& slot = idMap[index];
    slot.entity = entity;
    slot.generation++;
    slot.sectorId = sectorId;
    EntityId entityId = {index, slot.generation};
    return entityId;
}

bool RegistryMapping::unregisterEntityId(EntityId entityId)
{
    if (!validId(entityId))
    {
        return false;
    }
    EntMapSlot& slot = idMap[entityId.index];
    slot.entity = entt::null;
    idMapFreeSlots.push_back(entityId.index);
    return true;
}

bool RegistryMapping::validId(EntityId entityId)
{
    return entityId.index < idMap.size()
           && idMap[entityId.index].generation == entityId.generation
           && idMap[entityId.index].entity != entt::null;
}

const EntMapSlot* RegistryMapping::getEntity(EntityId entityId)
{
    if (!validId(entityId))
    {
        return nullptr;
    }
    return &idMap[entityId.index];
}

} // namespace ecs