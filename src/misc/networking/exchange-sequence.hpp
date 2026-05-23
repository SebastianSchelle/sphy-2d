#ifndef EXCHANGE_SEQUENCE_HPP
#define EXCHANGE_SEQUENCE_HPP

#include <net-shared.hpp>
#include <std-inc.hpp>
#include <protocol.hpp>
#include <concurrentqueue.h>

using moodycamel::ConcurrentQueue;

namespace net
{

typedef std::function<void()> ErrorCallback;
typedef std::function<void()> SuccessCallback;
typedef std::function<void(bitsery::Serializer<OutputAdapter>& ser)> SerializeCallback;
typedef std::function<string(void)> InfoGeneratorCallback;
class Exchange
{
  public:
    Exchange(uint16_t cmd,
             ErrorCallback errorCallback,
             SuccessCallback successCallback,
             SerializeCallback serializeCallback,
             string status,
             InfoGeneratorCallback infoGenerator);
    ~Exchange();
    void addCommand(uint16_t cmd);
    void execute(ConcurrentQueue<net::CmdQueueData>& sendQueue);

    uint16_t command;
    ErrorCallback errorCallback;
    SuccessCallback successCallback;
    SerializeCallback serializeCallback;
    string status;
    InfoGeneratorCallback infoGenerator;
};

class ExchangeSequence
{
  public:
    ExchangeSequence();
    ~ExchangeSequence();
    void registerExchange(Exchange exchange);
    void start(ConcurrentQueue<net::CmdQueueData>& sendQueue);
    void advance(ConcurrentQueue<net::CmdQueueData>& sendQueue, uint16_t recCommand, prot::cmd::State recState);
    bool done();
    void reset();
    Exchange& getCurrentExchange() { return exchanges[currentExchange]; }

  private:
    std::vector<Exchange> exchanges;
    uint16_t currentExchange;
};

}  // namespace net

#endif