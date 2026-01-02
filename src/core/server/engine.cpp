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
        PREP_SREQ_S(SendType::UDP, 0, prot::cmd::CMD_LOG, 0, 14)
        std::string str = "Hello World!\n";
        cmdser.text1b(str, str.size());
        cmdData.data.resize(cmdser.adapter().writtenBytesCount());
        sendQueue.enqueue(cmdData);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}  // namespace sphys
