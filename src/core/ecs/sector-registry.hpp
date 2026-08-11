#ifndef SECTOR_REGISTRY_HPP
#define SECTOR_REGISTRY_HPP

#include "entt/entt.hpp"
#include <comp-ident.hpp>
#include <cstdint>

namespace ecs
{

class RegistryMapping;

typedef std::function<bool(entt::registry&, entt::entity, ecs::EntityId)> SpawnCallback;

class SectorRegistry
{
  public:
    SectorRegistry();
    ~SectorRegistry();
    void init(RegistryMapping* registryMapping, uint32_t sectorId);
    bool moveEntity(const EntityId& entityId);
    bool spawnObject(const SpawnCallback& spwnClb);
    bool destroyObject(const EntityId& entityId);

  private:
    uint32_t sectorId;
    entt::registry registry;
    RegistryMapping* registryMapping;
};
}  // namespace ecs

#endif  // SECTOR_REGISTRY_HPP