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

typedef std::function<void(bitsery::Serializer<OutputAdapter>&)>
    CmdContentWriter;

void writeMessageUdp(ConcurrentQueue<net::CmdQueueData>& sendQueue,
                     const udp::endpoint* endpoint,
                     CmdContentWriter contentWriter, bool useToken = false, uint16_t removeTrailingBytes = 0);
void writeMessageTcp(ConcurrentQueue<net::CmdQueueData>& sendQueue,
                     std::shared_ptr<net::TcpConnection> tcpConnection,
                     CmdContentWriter contentWriter);
void writeMessage(net::CmdQueueData& cmdData,
                  CmdContentWriter contentWriter, bool useToken = false, uint16_t removeTrailingBytes = 0);
void writeCommand(bitsery::Serializer<OutputAdapter>& cmdser,
                  uint16_t cmd,
                  uint8_t flags,
                  CmdContentWriter contentWriter);

namespace cmd
{

enum class State
{
    SUCCESS,
    FAILURE,
};

const uint16_t LOG = 0x0001;
const uint16_t TIME_SYNC = 0x0002;
const uint16_t AUTHENTICATE = 0x0003;
const uint16_t WORLD_INFO = 0x0004;
const uint16_t CONSOLE_CMD = 0x0005;
const uint16_t VERSION_CHECK = 0x0006;
const uint16_t SLOW_DUMP = 0x0007;
const uint16_t REQ_ALL_COMPONENTS = 0x0008;

}  // namespace cmd

}  // namespace prot

#endif