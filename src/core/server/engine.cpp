#include "bitsery/serializer.h"
#include <engine.hpp>
#include <server.hpp>
#include <protocol.hpp>

namespace sphys
{

Engine::Engine()
{
}

Engine::~Engine() {}

void Engine::start()
{
    engineThread = std::thread([this]() { engineLoop(); });
}

void Engine::engineLoop()
{
    while(true)
    {
        CmdQueueData recQueueData;
        while (receiveQueue.try_dequeue(recQueueData))
        {
            parseCommand(recQueueData.data);
        }

        CMDAT_PREP_S(SendType::UDP, 0, prot::cmd::CMD_LOG, 0)
        std::string str = "Hello World!";
        cmdser.text1b(str, str.size());
        CMDAT_FIN_S()

        sendQueue.enqueue(cmdData);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Engine::parseCommand(std::vector<uint8_t> data)
{
    uint16_t cmd;
    uint8_t flags;
    uint16_t len;
    std::string uuid;
    bitsery::Deserializer<InputAdapter> cmddes(
        InputAdapter{data.begin(), data.size()});
    cmddes.text1b(uuid, 16);
    cmddes.value2b(cmd);
    cmddes.value1b(flags);
    cmddes.value2b(len);

    switch (cmd)
    {
        case prot::cmd::CMD_LOG:
        {
            std::string str;
            cmddes.text1b(str, len);
            LG_I("Log from uuid={}: {}", uuid, str);
            break;
        }
        default:
            break;
    }
}

}  // namespace sphys
