#include <client-registry.hpp>
#include <protocol.hpp>

namespace ecs
{


ClientRegistry::ClientRegistry(ConcurrentQueue<net::CmdQueueData>& sendQueue)
    : sendQueue(sendQueue)
{
}

ClientRegistry::~ClientRegistry() {}

entt::entity ClientRegistry::enttFromServerId(const EntityId& entityId,
                                              bool reqIfNone)
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

uint32_t ClientRegistry::getNumClientEntities() const
{
    return numClientEntities;
}

entt::registry& ClientRegistry::getRegistry()
{
    return registry;
}


bool ClientRegistry::validId(EntityId entityId)
{
    auto it = idMap.find(entityId.index);
    if (it == idMap.end())
    {
        return false;
    }
    return it->second.generation == entityId.generation
           && it->second.entity != entt::null;
}

entt::entity ClientRegistry::getEntity(EntityId entityId, bool reqIfNone)
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

void ClientRegistry::clearSession()
{
    registry.clear();
    idMap.clear();
}

void ClientRegistry::destroyServerEntity(EntityId entityId)
{
    auto it = idMap.find(entityId.index);
    if (it != idMap.end() && it->second.generation == entityId.generation
        && it->second.entity != entt::null)
    {
        destroyClientEntity(entityId.index);
    }
}

void ClientRegistry::destroyClientEntity(uint32_t index)
{
    registry.destroy(idMap[index].entity);
    idMap[index].entity = entt::null;
    numClientEntities--;
}

}  // namespace ecs
