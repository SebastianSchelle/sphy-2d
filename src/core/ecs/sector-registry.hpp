#ifndef SECTOR_REGISTRY_HPP
#define SECTOR_REGISTRY_HPP

#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include "registry-mapping.hpp"
#include <comp-ident.hpp>
#include <cstdint>

namespace world
{
class Sector;
}

namespace ecs
{

class RegistryMapping;

typedef std::function<bool(entt::registry&, entt::entity, ecs::EntityId)>
    SpawnCallback;

class SectorRegistry
{
  public:
    SectorRegistry();
    ~SectorRegistry();
    void init(RegistryMapping* registryMapping, world::Sector* sector);
    bool migrateEntity(EntityId entityId,
                       const EntMapSlot* slot,
                       SectorRegistry* lastRegistry);
    bool spawnObject(const SpawnCallback& spwnClb);
    bool destroyObject(EntityId entityId);

    entt::registry* getRegistry()
    {
        return &registry;
    }

  private:
    world::Sector* sector;
    entt::registry registry;
    RegistryMapping* registryMapping;
};
}  // namespace ecs

#endif  // SECTOR_REGISTRY_HPP