#ifndef REGISTRY_MAPPING_HPP
#define REGISTRY_MAPPING_HPP

#include <comp-ident.hpp>
#include <entt/entt.hpp>
#include <world-def.hpp>

namespace ecs
{

struct EntMapSlot
{
    entt::entity entity = entt::null;
    uint16_t generation = 0;
    uint32_t sectorId = world::INVALID_SECTOR_ID;
};

class RegistryMapping
{
  public:
    RegistryMapping();
    ~RegistryMapping();

    // Manage EntityId lifecycle
    EntityId registerEntity(uint32_t sectorId, entt::entity entity);
    bool unregisterEntityId(EntityId entityId);
    bool updateEntitySector(EntityId entityId, uint32_t sectorId, entt::entity entity);
    bool validId(EntityId entityId);
    const EntMapSlot* getEntity(EntityId entityId);

  private:
    vector<EntMapSlot> idMap;
    vector<uint32_t> idMapFreeSlots;
};

}  // namespace ecs

#endif  // REGISTRY_MAPPING_HPP