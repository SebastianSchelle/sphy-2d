#include "config-manager/config-manager.hpp"
#include <engine.hpp>
#include <server.hpp>

namespace sphys
{

Engine::Engine()
    : configManager("defs/config-server.yaml")
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
        SendRequest req;
        req.address = asio::ip::address::from_string("0.0.0.0");
        req.type = SendType::UDP;
        for(char c : "Hello World!\n")
        {
            req.data.push_back(c);
        }
        //SendRequest request;
        sendQueue.enqueue(req);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}  // namespace sphys
