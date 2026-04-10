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
    if(writeMessage(cmdData, contentWriter, useToken, removeTrailingBytes))
    {
        sendQueue.enqueue(cmdData);
    }
}

void writeMessageTcp(ConcurrentQueue<net::CmdQueueData>& sendQueue,
                     std::shared_ptr<net::TcpConnection> tcpConnection,
                     CmdContentWriter contentWriter)
{
    net::CmdQueueData cmdData;
    cmdData.sendType = net::SendType::TCP;
    cmdData.tcpConnection = tcpConnection;
    if(writeMessage(cmdData, contentWriter))
    {
        sendQueue.enqueue(cmdData);
    }
}

bool writeMessage(net::CmdQueueData& cmdData,
                  CmdContentWriter contentWriter, bool useToken, uint16_t removeTrailingBytes)
{
    bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(cmdData.data));
    if(cmdData.sendType == net::SendType::UDP && useToken)
    {
        cmdser.adapter().currentWritePos(17);
    }
    size_t ptrBefore = cmdser.adapter().currentWritePos();
    if(!contentWriter(cmdser))
    {
        return false;
    }
    size_t currPos = cmdser.adapter().currentWritePos();
    cmdData.data.resize(currPos - removeTrailingBytes);
    return true;
}

bool writeCommand(bitsery::Serializer<OutputAdapter>& cmdser,
                  uint16_t cmd,
                  uint8_t flags,
                  CmdContentWriter contentWriter)
{
    size_t ptrBefore = cmdser.adapter().currentWritePos();
    cmdser.value2b(cmd);
    cmdser.value1b(flags);
    size_t lenPos = cmdser.adapter().currentWritePos();
    cmdser.value2b((uint16_t)0);
    if(!contentWriter(cmdser))
    {
        return false;
    }
    size_t currPos = cmdser.adapter().currentWritePos();
    cmdser.adapter().currentWritePos(lenPos);
    cmdser.value2b((uint16_t)(currPos - lenPos - 2));
    cmdser.adapter().currentWritePos(currPos);
    return true;
}

}  // namespace prot
