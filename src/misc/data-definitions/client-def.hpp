#ifndef CLIENT_DEF_HPP
#define CLIENT_DEF_HPP

#include <comp-ident.hpp>
#include <item-lib.hpp>
#include <net-shared.hpp>
#include <std-inc.hpp>

namespace def
{

struct ClientFlags
{
    uint8_t enConsole : 1;
};

class ClientInfo
{
  public:
#ifdef SERVER
    ClientInfo(const std::string& name,
               const net::ClientInfo& clientInfo,
               const ClientFlags& flags)
    {
        this->name = name;
        this->clientInfo = clientInfo;
        this->lastSlowDump = tim::nowU();
        this->flags = flags;
    }
#endif
#ifdef CLIENT
    ClientInfo() {}
    ClientInfo(const std::string& name,
               const net::ModelClientInfo& modelClientInfo,
               const ClientFlags& flags)
    {
        this->name = name;
        this->modelClientInfo = modelClientInfo;
        this->flags = flags;
    }
#endif
    ~ClientInfo(){}
#ifdef SERVER
    net::ClientInfo clientInfo;
    long lastSlowDump;
#endif
#ifdef CLIENT
    net::ModelClientInfo modelClientInfo;
#endif

    void setActiveEntity(const ecs::EntityId& activeEntity)
    {
        this->activeEntity = activeEntity;
    }

    const std::string& getName() const
    {
        return name;
    }

    const ecs::EntityId& getActiveEntity() const
    {
        return activeEntity;
    }

    const ClientFlags& getFlags() const
    {
        return flags;
    }

  private:
    std::string name;
    ecs::EntityId activeEntity;
    ClientFlags flags;
};

using ClientInfoHandle = typename con::ItemLib<ClientInfo>::Handle;

}  // namespace def

#endif