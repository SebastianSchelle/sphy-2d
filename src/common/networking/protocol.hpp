#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <std-inc.hpp>

using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

#define CMDAT_PREP_S(sType, cId, cmd, flags)                                   \
    CmdQueueData cmdData;                                                      \
    cmdData.sendType = sType;                                                  \
    cmdData.clientId = cId;                                                    \
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));    \
    cmdser.value2b((uint16_t)cmd);                                             \
    cmdser.value1b((uint8_t)flags);                                            \
    cmdser.adapter().currentWritePos(5);
#define CMDAT_FIN_S()                                                          \
    cmdser.adapter().currentWritePos(3);                                       \
    cmdser.value2b((uint16_t)(cmdser.adapter().writtenBytesCount() - 5));      \
    cmdData.data.resize(cmdser.adapter().writtenBytesCount());


#define CMDAT_PREP_C(sType, cmd, flags)                                        \
    CmdQueueData cmdData;                                                      \
    cmdData.sendType = sType;                                                  \
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));    \
    cmdser.adapter().currentWritePos(17);                                      \
    cmdser.value2b((uint16_t)cmd);                                             \
    cmdser.value1b((uint8_t)flags);                                            \
    cmdser.adapter().currentWritePos(22);
#define CMDAT_FIN_C()                                                          \
    cmdser.adapter().currentWritePos(20);                                      \
    cmdser.value2b((uint16_t)(cmdser.adapter().writtenBytesCount() - 22));     \
    cmdData.data.resize(cmdser.adapter().writtenBytesCount());

namespace prot
{

namespace cmd
{
const uint16_t CMD_LOG = 0x0001;
}

}  // namespace prot

#endif