#include <exchange-sequence.hpp>

namespace net
{

ExchangeSequence::ExchangeSequence()
{
}

ExchangeSequence::~ExchangeSequence()
{
}

void ExchangeSequence::registerExchange(Exchange exchange)
{
    exchanges.push_back(exchange);
}

void ExchangeSequence::start(ConcurrentQueue<net::CmdQueueData>& sendQueue)
{
    currentExchange = 0;
    exchanges[currentExchange].execute(sendQueue);
}

void ExchangeSequence::advance(ConcurrentQueue<net::CmdQueueData>& sendQueue, uint16_t recCommand, prot::cmd::State recState)
{
    Exchange& curr = exchanges[currentExchange];
    if(curr.command == recCommand)
    {
        if(recState == prot::cmd::State::SUCCESS)
        {
            curr.successCallback();
            if(currentExchange < exchanges.size() - 1)
            {
                currentExchange++;
                exchanges[currentExchange].execute(sendQueue);
            }
        }
        else
        {
            curr.errorCallback();
        }
    }
}

bool ExchangeSequence::done()
{
    return currentExchange >= exchanges.size();
}

Exchange::Exchange(uint16_t cmd,
                   ErrorCallback errorCallback,
                   SuccessCallback successCallback,
                   SerializeCallback serializeCallback)
    : command(cmd), errorCallback(errorCallback), successCallback(successCallback), serializeCallback(serializeCallback)
{
}

Exchange::~Exchange()
{
}

void Exchange::execute(ConcurrentQueue<net::CmdQueueData>& sendQueue)
{
    CMDAT_PREP(net::SendType::TCP, command, 0)
    serializeCallback(cmdser);
    CMDAT_FIN_TOKEN()
    sendQueue.enqueue(cmdData);
}


}  // namespace net