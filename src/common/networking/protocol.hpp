#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <net-shared.hpp>
#include <std-inc.hpp>

#define CMD_FLAG_RESP 0x01

using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

#define CMDAT_PREP(sType, cmd, flags)                                     \
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

namespace cmd
{
const uint16_t LOG = 0x0001;
const uint16_t TIME_SYNC = 0x0002;
const uint16_t CONNECT = 0x0003;
}  // namespace cmd

}  // namespace prot

#endif