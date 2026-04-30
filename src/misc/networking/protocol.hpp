#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <net-shared.hpp>
#include <std-inc.hpp>

#define CMD_FLAG_RESP 0x01

#define CMDAT_PREP(sType, cmd, flags)                                          \
    net::CmdQueueData cmdData;                                                 \
    cmdData.sendType = sType;                                                  \
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));    \
    cmdser.value2b((uint16_t)cmd);                                             \
    cmdser.value1b((uint8_t)flags);                                            \
    cmdser.adapter().currentWritePos(5);
#define CMDAT_FIN()                                                            \
    cmdser.adapter().currentWritePos(3);                                       \
    cmdser.value2b((uint16_t)(cmdser.adapter().writtenBytesCount() - 5));      \
    cmdData.data.resize(cmdser.adapter().writtenBytesCount());
#define CMDAT_FIN_REM_TAIL_BYTES(removeTailBytes)                              \
    cmdser.adapter().currentWritePos(3);                                       \
    cmdser.value2b((uint16_t)(cmdser.adapter().writtenBytesCount() - 5         \
                              - (removeTailBytes)));                           \
    cmdData.data.resize(cmdser.adapter().writtenBytesCount()                   \
                        - (removeTailBytes));

#define CMDAT_PREP_TOKEN(sType, cmd, flags)                                    \
    net::CmdQueueData cmdData;                                                 \
    cmdData.sendType = sType;                                                  \
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));    \
    cmdser.adapter().currentWritePos(17);                                      \
    cmdser.value2b((uint16_t)cmd);                                             \
    cmdser.value1b((uint8_t)flags);                                            \
    cmdser.adapter().currentWritePos(22);
#define CMDAT_FIN_TOKEN()                                                      \
    cmdser.adapter().currentWritePos(20);                                      \
    cmdser.value2b((uint16_t)(cmdser.adapter().writtenBytesCount() - 22));     \
    cmdData.data.resize(cmdser.adapter().writtenBytesCount());


namespace prot
{

inline constexpr std::size_t kMaxSerializedChunkBytes = 1000;

typedef std::function<bool(bitsery::Serializer<OutputAdapter>&)>
    CmdContentWriter;

void writeMessageUdp(ConcurrentQueue<net::CmdQueueData>& sendQueue,
                     const udp::endpoint* endpoint,
                     CmdContentWriter contentWriter,
                     bool useToken = false,
                     uint16_t removeTrailingBytes = 0);
void writeMessageTcp(ConcurrentQueue<net::CmdQueueData>& sendQueue,
                     net::TcpConnection* tcpConnection,
                     CmdContentWriter contentWriter);
bool writeMessage(net::CmdQueueData& cmdData,
                  CmdContentWriter contentWriter,
                  bool useToken = false,
                  uint16_t removeTrailingBytes = 0);
bool writeCommand(bitsery::Serializer<OutputAdapter>& cmdser,
                  uint16_t cmd,
                  uint8_t flags,
                  CmdContentWriter contentWriter);

class MsgComposer
{
  public:
    MsgComposer(net::SendType type, const udp::endpoint& endpoint, bool useToken = true);
    MsgComposer(net::SendType type, net::TcpConnection* tcpConnection);
    ~MsgComposer();
    void resetData();
    void startCommand(uint16_t cmd, uint8_t flags);
    void execute(ConcurrentQueue<net::CmdQueueData>& sendQueue);
    bool hasData() const { return hasContent; }
    bitsery::Serializer<OutputAdapter>* ser = nullptr;
  private:
    void finishCommand();
    net::CmdQueueData cmdData;
    size_t currCmdPos = 0;
    size_t currLenPos = 0;
    bool hasContent = false;
    bool cmdFinished = true;
};

namespace cmd
{

enum class State
{
    SUCCESS,
    FAILURE,
};

struct MoveToFlags
{
    uint8_t queue : 1 = 0;
};

const uint16_t LOG = 0x0001;
const uint16_t TIME_SYNC = 0x0002;
const uint16_t AUTHENTICATE = 0x0003;
const uint16_t WORLD_INFO = 0x0004;
const uint16_t CONSOLE_CMD = 0x0005;
const uint16_t VERSION_CHECK = 0x0006;
const uint16_t SLOW_DUMP = 0x0007;
const uint16_t REQ_ALL_COMPONENTS = 0x0008;
const uint16_t SEL_CMD_MOVETO = 0x0009;
const uint16_t NOTIFY_CLIENT_READY = 0x000A;
const uint16_t DBG_GET_AABB_TREE = 0x000B;
const uint16_t ACTIVE_ENTITY_SWITCHED = 0x000C;
const uint16_t ALL_ENTT_COMPONENTS = 0x000D;
const uint16_t ACTIVE_SECTOR_UPDATE = 0x000E;

}  // namespace cmd

}  // namespace prot

#endif