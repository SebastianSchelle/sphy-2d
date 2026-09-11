#ifndef CLIENT_DEF_HPP
#define CLIENT_DEF_HPP

#include "world-def.hpp"
#include <comp-ident.hpp>
#include <control-def.hpp>
#include <item-lib.hpp>
#include <net-shared.hpp>
#include <std-inc.hpp>
#include <work-sequencer.hpp>

namespace def
{

static constexpr size_t CLIENT_INFO_NAME_MAX = 256;

namespace Dbg
{
typedef uint16_t Flags;
constexpr Flags None = 0;
constexpr Flags enCollAvoidInfo = 0x0001;
constexpr Flags enConsole = 0x0002;
}  // namespace Dbg

struct ClientViewRect
{
    gfx::GameViewMode viewMode;
    float zoom;
    SectorCoords tl;
    SectorCoords br;

    bool operator==(const ClientViewRect& other) const
    {
        return viewMode == other.viewMode && tl == other.tl && br == other.br && zoom == other.zoom;
    }
    bool operator!=(const ClientViewRect& other) const
    {
        return !(*this == other);
    }
};

#define SER_CLIENT_VIEW_RECT                                                   \
    SOBJ(o.viewMode);                                                          \
    S4b(o.zoom);                                                               \
    SOBJ(o.tl);                                                                \
    SOBJ(o.br);
EXT_SER(ClientViewRect, SER_CLIENT_VIEW_RECT)
EXT_DES(ClientViewRect, SER_CLIENT_VIEW_RECT)

class ClientInfo
{
  public:
#ifdef SERVER
    ClientInfo(const std::string& name,
               const net::ClientInfo& clientInfo,
               Dbg::Flags flags)
        : workSequencer(10000)
    {
        this->name = name;
        this->clientInfo = clientInfo;
        this->dbgFlags = flags;
        thirdPersonControl.flags = 0;
        lastClientUpdMap = tim::nowU();
        lastClientUpdFast3rd = tim::nowU();
    }
#endif
#ifdef CLIENT
    ClientInfo() {}
    ClientInfo(const std::string& name,
               const net::ModelClientInfo& modelClientInfo,
               Dbg::Flags flags)
    {
        this->name = name;
        this->modelClientInfo = modelClientInfo;
        this->dbgFlags = flags;
    }
#endif
    ~ClientInfo() {}
#ifdef SERVER
    net::ClientInfo clientInfo;
    long lastClientUpdFast3rd;
    long lastClientUpdMap;
    ThirdPersonControl thirdPersonControl;

    void addWorkFunction(work::WorkFunction workFunction)
    {
        workSequencer.addWorkFunction(workFunction);
    }
    void ackWorkSequencer()
    {
        workSequencer.ack();
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
    uint32_t currentSector = 0;
    Dbg::Flags dbgFlags;
    std::string name;
    ClientViewRect clientViewRect;

  private:
    std::set<uint32_t> activeSectors;
#ifdef SERVER
    work::WorkSequencer workSequencer;
#endif
};

using ClientInfoHandle = typename con::ItemLib<ClientInfo>::Handle;

#define SER_CLIENT_INFO                                                        \
    SOBJ(o.activeEntity);                                                      \
    S2b(o.dbgFlags);                                                           \
    STXT(o.name, CLIENT_INFO_NAME_MAX);
EXT_SER(ClientInfo, SER_CLIENT_INFO)
EXT_DES(ClientInfo, SER_CLIENT_INFO)

}  // namespace def

#endif