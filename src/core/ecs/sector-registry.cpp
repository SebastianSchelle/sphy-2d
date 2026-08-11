#include "sector-registry.hpp"
#include "comp-ident.hpp"
#include "entt/entity/entity.hpp"
#include "logging.hpp"
#include <registry-mapping.hpp>

namespace ecs
{

SectorRegistry::SectorRegistry() {}

SectorRegistry::~SectorRegistry() {}

void SectorRegistry::init(RegistryMapping* registryMapping, uint32_t sectorId)
{
    this->registryMapping = registryMapping;
    this->sectorId = sectorId;
}

bool SectorRegistry::spawnObject(const SpawnCallback& spwnClb)
{
    entt::entity entity = registry.create();
    if (entity == entt::null)
    {
        LG_W("Could not create entity in sector {}", sectorId);
        return false;
    }
    EntityId entityId = registryMapping->registerEntity(sectorId, entity);
    if (entityId == EntityId::Invalid())
    {
        LG_W("Could not create EntityId in registryMap");
        registry.destroy(entity);
        return false;
    }
    // Both Entity and entityId exist now, populate object
    registry.emplace<EntityId>(entity, entityId);
    registry.emplace<ecs::Flags>(entity);
    if (spwnClb)
    {
        bool res = spwnClb(registry, entity, entityId);
        if (!res)
        {
            registryMapping->unregisterEntityId(entityId);
            registry.destroy(entity);
            LG_W("Spawn callback return fault. Destroyed unfinished Entity");
        }
    }
    return true;
}

bool SectorRegistry::destroyObject(const EntityId& entityId)
{
    const EntMapSlot* slot = registryMapping->getEntity(entityId);
    if (!slot)
    {
        LG_W("Slot invalid. Could not destroy object {}", entityId);
        return false;
    }
    registry.destroy(slot->entity);

    // todo: how to unlink all references? Or will this be done dynamically when
    // used?

    bool res = registryMapping->unregisterEntityId(entityId);
    if (!res)
    {
        LG_W("Unregistering entityId {} failed", entityId);
        return false;
    }
    return true;
}

}  // namespace ecs