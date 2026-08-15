#ifndef CLIENT_REGISTRY_HPP
#define CLIENT_REGISTRY_HPP

#include "std-inc.hpp"
#include <net-shared.hpp>
#include <entt/entt.hpp>
#include <comp-ident.hpp>

namespace ecs
{

struct ClientSlot
{
    game_entity entity;
    uint16_t generation;
};

class ClientRegistry
{
  public:
    ClientRegistry(ConcurrentQueue<net::CmdQueueData>& sendQueue);
    ~ClientRegistry();
    Registry& getRegistry();
    game_entity enttFromServerId(const EntityId& entityId,
                                  bool reqIfNone = true);
    game_entity getEntity(EntityId entityId, bool reqIfNone = true);
    bool validId(EntityId entityId);
    void clearSession();
    uint32_t getNumClientEntities() const;
    void destroyServerEntity(EntityId entityId);

    uint32_t numServerEntities = 0;

  private:
    void destroyClientEntity(uint32_t index);

    Registry registry;
    std::unordered_map<uint32_t, ClientSlot> idMap;
    uint32_t numClientEntities = 0;
    ConcurrentQueue<net::CmdQueueData>& sendQueue;
};

}  // namespace ecs

#endif