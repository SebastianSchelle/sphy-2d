#ifndef CLIENT_REGISTRY_HPP
#define CLIENT_REGISTRY_HPP

#include <net-shared.hpp>
#include <entt/entt.hpp>
#include <comp-ident.hpp>

namespace ecs
{

struct ClientSlot
{
    entt::entity entity;
    uint16_t generation;
};

class ClientRegistry
{
  public:
    ClientRegistry(ConcurrentQueue<net::CmdQueueData>& sendQueue);
    ~ClientRegistry();
    entt::registry& getRegistry();
    entt::entity enttFromServerId(const EntityId& entityId,
                                  bool reqIfNone = true);
    entt::entity getEntity(EntityId entityId, bool reqIfNone = true);
    bool validId(EntityId entityId);
    void clearSession();
    uint32_t getNumClientEntities() const;
    void destroyServerEntity(EntityId entityId);

    uint32_t numServerEntities = 0;

  private:
    void destroyClientEntity(uint32_t index);

    entt::registry registry;
    std::unordered_map<uint32_t, ClientSlot> idMap;
    uint32_t numClientEntities = 0;
    ConcurrentQueue<net::CmdQueueData>& sendQueue;
};

}  // namespace ecs

#endif