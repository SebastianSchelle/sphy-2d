#include "protocol.hpp"
#include <concurrentqueue.h>

namespace prot
{

void writeMessageUdp(ConcurrentQueue<net::CmdQueueData>& sendQueue,
                     const udp::endpoint* endpoint,
                     CmdContentWriter contentWriter, bool useToken, uint16_t removeTrailingBytes)
{
    net::CmdQueueData cmdData;
    cmdData.sendType = net::SendType::UDP;
    if(endpoint)
    {
        cmdData.udpEndpoint = *endpoint;
    }
    writeMessage(cmdData, contentWriter, useToken, removeTrailingBytes);
    sendQueue.enqueue(cmdData);
}

void writeMessageTcp(ConcurrentQueue<net::CmdQueueData>& sendQueue,
                     std::shared_ptr<net::TcpConnection> tcpConnection,
                     CmdContentWriter contentWriter)
{
    net::CmdQueueData cmdData;
    cmdData.sendType = net::SendType::TCP;
    cmdData.tcpConnection = tcpConnection;
    writeMessage(cmdData, contentWriter);
    sendQueue.enqueue(cmdData);
}

void writeMessage(net::CmdQueueData& cmdData,
                  CmdContentWriter contentWriter, bool useToken, uint16_t removeTrailingBytes)
{
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));
    if(cmdData.sendType == net::SendType::UDP && useToken)
    {
        cmdser.adapter().currentWritePos(17);
    }
    contentWriter(cmdser);
    cmdData.data.resize(cmdser.adapter().writtenBytesCount() - removeTrailingBytes);
}

void writeCommand(bitsery::Serializer<OutputAdapter>& cmdser,
                  uint16_t cmd,
                  uint8_t flags,
                  CmdContentWriter contentWriter)
{
    cmdser.value2b(cmd);
    cmdser.value1b(flags);
    size_t lenPos = cmdser.adapter().currentWritePos();
    cmdser.value2b((uint16_t)0);
    contentWriter(cmdser);
    cmdser.adapter().currentWritePos(lenPos);
    cmdser.value2b((uint16_t)(cmdser.adapter().writtenBytesCount() - lenPos - 2));
    cmdser.adapter().currentWritePos(cmdser.adapter().writtenBytesCount());
}

}  // namespace prot
