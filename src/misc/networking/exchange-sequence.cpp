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
    LG_I("Registered exchange: {}", exchange.command);
}

void ExchangeSequence::start(ConcurrentQueue<net::CmdQueueData>& sendQueue)
{
    currentExchange = 0;
    if(exchanges.size() > 0)
    {
        exchanges[currentExchange].execute(sendQueue);
    }
    else
    {
        LG_E("No exchanges registered");
    }
}

void ExchangeSequence::advance(ConcurrentQueue<net::CmdQueueData>& sendQueue, uint16_t recCommand, prot::cmd::State recState)
{
    Exchange& curr = exchanges[currentExchange];
    if(curr.command == recCommand)
    {
        if(recState == prot::cmd::State::SUCCESS)
        {
            curr.successCallback();
            currentExchange++;
            if(currentExchange < exchanges.size())
            {
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
    LG_I("Executing exchange: {}", command);
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(command, 0);
    serializeCallback(*mcomp.ser);
    mcomp.execute(sendQueue);
}


}  // namespace net