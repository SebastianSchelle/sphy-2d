#ifndef CLIENT_DEF_HPP
#define CLIENT_DEF_HPP

#include <comp-ident.hpp>
#include <control-def.hpp>
#include <item-lib.hpp>
#include <net-shared.hpp>
#include <std-inc.hpp>
#include <work-sequencer.hpp>

namespace def
{

#define CLIENT_FLAG_EN_CONSOLE 1 << 0
static constexpr size_t CLIENT_INFO_NAME_MAX = 256;

class ClientInfo
{
  public:
#ifdef SERVER
    ClientInfo(const std::string& name,
               const net::ClientInfo& clientInfo,
               uint8_t flags)
        : workSequencer(5000)
    {
        this->name = name;
        this->clientInfo = clientInfo;
        this->lastSlowDump = tim::nowU();
        this->lastActiveSectorDump = tim::nowU();
        this->flags = flags;
        this->thirdPersonControl.flags = 0;
    }
#endif
#ifdef CLIENT
    ClientInfo() {}
    ClientInfo(const std::string& name,
               const net::ModelClientInfo& modelClientInfo,
               uint8_t flags)
    {
        this->name = name;
        this->modelClientInfo = modelClientInfo;
        this->flags = flags;
    }
#endif
    ~ClientInfo() {}
#ifdef SERVER
    net::ClientInfo clientInfo;
    long lastSlowDump;
    long lastActiveSectorDump;
    ThirdPersonControl thirdPersonControl;

    void addWorkFunction(std::function<void()> workFunction)
    {
        workSequencer.addWorkFunction(workFunction);
    }
    void executeWorkSequencer()
    {
        workSequencer.execute();
    }
    void clearWorkSequencer()
    {
        workSequencer.clear();
    }
    const std::set<uint32_t>& getActiveSectors() const
    {
        return activeSectors;
    }
    void clearActiveSectors()
    {
        activeSectors.clear();
    }
    void addActiveSector(uint32_t sectorId)
    {
        activeSectors.insert(sectorId);
    }
#endif
#ifdef CLIENT
    net::ModelClientInfo modelClientInfo;
#endif

    ecs::EntityId activeEntity;
    uint8_t flags;
    std::string name;

  private:
    std::set<uint32_t> activeSectors;
#ifdef SERVER
    work::WorkSequencer workSequencer;
#endif
};

using ClientInfoHandle = typename con::ItemLib<ClientInfo>::Handle;

#define SER_CLIENT_INFO                                                        \
    SOBJ(o.activeEntity);                                                      \
    S1b(o.flags);                                                              \
    STXT(o.name, CLIENT_INFO_NAME_MAX);
EXT_SER(ClientInfo, SER_CLIENT_INFO)
EXT_DES(ClientInfo, SER_CLIENT_INFO)

}  // namespace def

#endif