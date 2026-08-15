#ifndef SECTOR_REGISTRY_HPP
#define SECTOR_REGISTRY_HPP

#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include "registry-mapping.hpp"
#include "task-system.hpp"
#include <comp-ident.hpp>
#include <cstdint>

namespace world
{
class Sector;
}

namespace ecs
{

class RegistryMapping;

struct SpawnCallbackParams
{
    entt::registry& reg;
    ai::TaskSystem& taskSystem;
    entt::entity entity;
    ecs::EntityId entityId;
};

typedef std::function<bool(SpawnCallbackParams& params)>
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
    EntityId spawnObject(const SpawnCallback& spwnClb);
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