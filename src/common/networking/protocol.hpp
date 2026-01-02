#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <std-inc.hpp>

using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

#define PREP_SREQ_S(sType, cId, cmd, flags, len)                               \
    CmdQueueData cmdData;                                                      \
    cmdData.sendType = sType;                                                  \
    cmdData.clientId = cId;                                                    \
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));    \
    cmdser.value2b((uint16_t)cmd);                                             \
    cmdser.value1b((uint8_t)flags);                                            \
    cmdser.value2b((uint16_t)len);

#define PREP_SREQ_C(uuid, sType, cmd, flags, len)                              \
    CmdQueueData cmdData;                                                      \
    cmdData.sendType = sType;                                                  \
    memcpy(cmdData.uuid, uuid, 16);                                            \
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));    \
    cmdser.value2b((uint16_t)cmd);                                             \
    cmdser.value1b((uint8_t)flags);                                            \
    cmdser.value2b((uint16_t)len);

namespace prot
{

namespace cmd
{
const uint16_t CMD_LOG = 0x0001;
}

}  // namespace prot

#endif