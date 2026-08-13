#include "sector-registry.hpp"
#include "comp-ident.hpp"
#include "entt/entity/entity.hpp"
#include "logging.hpp"
#include <registry-mapping.hpp>
#include <sector.hpp>

namespace ecs
{

SectorRegistry::SectorRegistry() {}

SectorRegistry::~SectorRegistry() {}

void SectorRegistry::init(RegistryMapping* registryMapping,
                          world::Sector* sector)
{
    this->registryMapping = registryMapping;
    this->sector = sector;
}

EntityId SectorRegistry::spawnObject(const SpawnCallback& spwnClb)
{
    entt::entity entity = registry.create();
    if (entity == entt::null)
    {
        LG_W("Could not create entity in sector {}", sector->getId());
        return EntityId::Invalid();
    }
    EntityId entityId =
        registryMapping->registerEntity(sector->getId(), entity);
    if (entityId == EntityId::Invalid())
    {
        LG_W("Could not create EntityId in registryMap");
        registry.destroy(entity);
        return EntityId::Invalid();
    }
    // Both Entity and entityId exist now, populate object
    registry.emplace<EntityId>(entity, entityId);
    registry.emplace<SectorId>(
        entity, sector->getId(), sector->getCoordX(), sector->getCoordY());
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
    return entityId;
}

bool SectorRegistry::migrateEntity(EntityId entityId,
                                   const EntMapSlot* slot,
                                   SectorRegistry* lastRegistry)
{
    if (!slot || !lastRegistry)
    {
        return false;
    }
    entt::entity newEntity = registry.create();
    
    // copy all components to this new entity

    // Update SectorId to reflect new sector
    auto sectorId = registry.emplace_or_replace<ecs::SectorId>(
        newEntity, sector->getId(), sector->getCoordX(), sector->getCoordY());

    // Update slot in global registry map and delete old entity in last sector
    if(!registryMapping->updateEntitySector(entityId, sector->getId(), newEntity))
    {
        LG_E("Couldn't update entities sector in global registry mapping");
        lastRegistry->getRegistry()->destroy(slot->entity);
        registry.destroy(newEntity);
        registryMapping->unregisterEntityId(entityId);
        return false;
    }
    lastRegistry->getRegistry()->destroy(slot->entity);
    return true;
}

bool SectorRegistry::destroyObject(EntityId entityId)
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