#include "ecs.hpp"
#include <protocol.hpp>

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
    slot.entity = e;
    slot.generation++;
    EntityId entityId = {index, slot.generation};
    registry.emplace<EntityId>(e, entityId);
    return entityId;
}

bool Ecs::destroyEntity(EntityId entityId)
{
    if (!validId(entityId))
    {
        return false;
    }
    Slot& slot = idMap[entityId.index];
    registry.destroy(slot.entity);
    slot.entity = entt::null;
    idMapFreeSlots.push_back(entityId.index);
    return true;
}

void Ecs::iterateEntities(IterateEntitiesCallback callback)
{
    for (uint32_t i = 0; i < idMap.size(); i++)
    {
        callback({i, idMap[i].generation});
    }
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

EntityId Ecs::getEntityIdFromIdx(uint32_t index)
{
    if (index < idMap.size())
    {
        return {index, idMap[index].generation};
    }
    return {0, 0};
}

EntityId Ecs::getEntityId(entt::entity entity)
{
    return registry.get<EntityId>(entity);
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

EntityId Ecs::spawnEntityFromAsset(const std::string& assetId,
                                   const AssetFactory& assetFactory)
{
    return assetFactory.createFromAsset(*this, assetId);
}

entt::registry& Ecs::getRegistry()
{
    return registry;
}

uint32_t Ecs::getNumEntities() const
{
    return idMap.size() - idMapFreeSlots.size();
}

EcsClient::EcsClient(ConcurrentQueue<net::CmdQueueData>& sendQueue)
    : sendQueue(sendQueue)
{
}

EcsClient::~EcsClient() {}

entt::entity EcsClient::enttFromServerId(const EntityId& entityId, bool reqIfNone)
{
    auto it = idMap.find(entityId.index);
    if (it == idMap.end() || it->second.generation == 0)
    {
        // Create new entity
        entt::entity e = registry.create();
        idMap[entityId.index] = {e, entityId.generation};
        numClientEntities++;
        if (reqIfNone)
        {
            prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
            mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, 0);
            mcomp.ser->object(entityId);
            mcomp.execute(sendQueue);
        }
        return e;
    }
    else if (it->second.generation != entityId.generation)
    {
        // Generation mismatch, destroy old entity and create new one
        if (it->second.entity != entt::null)
        {
            destroyClientEntity(entityId.index);
        }
        auto e = registry.create();
        idMap[entityId.index] = {e, entityId.generation};
        numClientEntities++;
        if (reqIfNone)
        {
            prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
            mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, 0);
            mcomp.ser->object(entityId);
            mcomp.execute(sendQueue);
        }
        return e;
    }
    else
    {
        // Generation matches, return existing entity
        return it->second.entity;
    }
}

uint32_t EcsClient::getNumClientEntities() const
{
    return numClientEntities;
}

entt::registry& EcsClient::getRegistry()
{
    return registry;
}


bool EcsClient::validId(EntityId entityId)
{
    auto it = idMap.find(entityId.index);
    if (it == idMap.end())
    {
        return false;
    }
    return it->second.generation == entityId.generation
           && it->second.entity != entt::null;
}

entt::entity EcsClient::getEntity(EntityId entityId, bool reqIfNone)
{
    if (!validId(entityId))
    {
        if (reqIfNone)
        {
            prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
            mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, 0);
            mcomp.ser->object(entityId);
            mcomp.execute(sendQueue);
        }
        return entt::null;
    }
    return idMap[entityId.index].entity;
}

void EcsClient::clearSession()
{
    registry.clear();
    idMap.clear();
}

void EcsClient::destroyServerEntity(EntityId entityId)
{
    auto it = idMap.find(entityId.index);
    if (it != idMap.end() && it->second.generation == entityId.generation
        && it->second.entity != entt::null)
    {
        destroyClientEntity(entityId.index);
    }
}

void EcsClient::destroyClientEntity(uint32_t index)
{
    registry.destroy(idMap[index].entity);
    idMap[index].entity = entt::null;
    numClientEntities--;
}

}  // namespace ecs