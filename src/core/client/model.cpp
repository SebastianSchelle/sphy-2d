#include "logging.hpp"
#include "std-inc.hpp"
#include <exchange-sequence.hpp>
#include <model.hpp>
#include <protocol.hpp>
#include <ui/user-interface.hpp>
#include <world-def.hpp>
#include <version.hpp>

namespace sphyc
{

Model::Model(ui::UserInterface* userInterface) : userInterface(userInterface)
{
    loadWorldSequence.registerExchange(net::Exchange(
        prot::cmd::WORLD_INFO,
        [this]() {},
        [this]() {},
        [this](bitsery::Serializer<OutputAdapter>& ser) {}));
    lastTSync = tim::getCurrentTimeU();
}

Model::~Model() {}

void Model::modelLoop(float dt)
{
    net::CmdQueueData recQueueData;
    while (receiveQueue.try_dequeue(recQueueData))
    {
        parseCommand(recQueueData.data);
    }
    switch (gameState)
    {
        case ClientGameState::Init:
            break;
        case ClientGameState::MainMenu:
            modelLoopMenu(dt);
            break;
        case ClientGameState::VersionCheck:
            break;
        case ClientGameState::Authenticated:
            loadWorldSequence.start(sendQueue);
            gameState = ClientGameState::LoadWorld;
            break;
        case ClientGameState::LoadWorld:
            if (loadWorldSequence.done())
            {
                LG_I("Exchanging world info with server done");
                gameState = ClientGameState::GameLoop;
            }
            break;
        case ClientGameState::GameLoop:
            modelLoopGame(dt);
            break;
        default:
            break;
    }
}

void Model::startLoadingMods()
{
    gameState = ClientGameState::LoadingMods;
}

void Model::startModel()
{
    gameState = ClientGameState::MainMenu;
}

void Model::timeSync()
{
    if (timeSyncData.waiting)
    {
        if ((tim::nowU() - timeSyncData.t0) > 1000000)
        {
            timeSyncData.waiting = false;
        }
        else
        {
            return;
        }
    }
    timeSyncData.t0 = tim::nowU();
    timeSyncData.waiting = true;
    CMDAT_PREP_TOKEN(net::SendType::UDP, prot::cmd::TIME_SYNC, 0)
    CMDAT_FIN_TOKEN()
    sendQueue.enqueue(cmdData);
}

void Model::modelLoopMenu(float dt) {}

void Model::modelLoopGame(float dt)
{
    tim::Timepoint now = tim::getCurrentTimeU();

    // Send some stuff to server
    /*CMDAT_PREP_TOKEN(net::SendType::UDP, prot::cmd::LOG, 0)
    std::string str = "Hello World!";
    cmdser.text1b(str, str.size());
    CMDAT_FIN_TOKEN()
    sendQueue.enqueue(cmdData);*/

    if (timeSyncData.cnt == 0)
    {
        DO_PERIODIC_EXTNOW(lastTSync, 2000000, now, [this]() { timeSync(); });
    }
    else
    {
        DO_PERIODIC_EXTNOW(lastTSync, 50000, now, [this]() { timeSync(); });
    }
}

void Model::parseCommand(std::vector<uint8_t> data)
{
    uint16_t cmd;
    uint8_t flags;
    uint16_t len;
    bitsery::Deserializer<InputAdapter> cmddes(
        InputAdapter{data.begin(), data.size()});
    cmddes.value2b(cmd);
    cmddes.value1b(flags);
    cmddes.value2b(len);
    prot::cmd::State result = prot::cmd::State::SUCCESS;

    switch (cmd)
    {
        case prot::cmd::LOG:
        {
            std::string str;
            cmddes.text1b(str, len);
            LG_I("Log from server: {}", str);
            break;
        }
        case prot::cmd::TIME_SYNC:
        {
            if (timeSyncData.waiting && flags & CMD_FLAG_RESP && len == 8)
            {
                timeSyncData.waiting = false;
                // Server time at request arrival
                cmddes.value8b(timeSyncData.t1);
                // Now
                timeSyncData.t2 = tim::nowU();
                // Travel time from client to server and back again
                long rtt = timeSyncData.t2 - timeSyncData.t0;

                // latency = half travel time
                timeSyncData.latency[timeSyncData.cnt] = rtt / 2;
                // Server time = server time at request arrival + latency
                long serverTime =
                    timeSyncData.t1 + timeSyncData.latency[timeSyncData.cnt];
                timeSyncData.offset[timeSyncData.cnt] =
                    serverTime - timeSyncData.t2;
                timeSyncData.cnt++;
                if (timeSyncData.cnt == 10)
                {
                    long latMin = 1000000000;
                    long offsMin;
                    for (uint i = 0; i < 10; ++i)
                    {
                        if (timeSyncData.latency[i] < latMin)
                        {
                            latMin = timeSyncData.latency[i];
                            offsMin = timeSyncData.offset[i];
                        }
                    }
                    timeSyncData.serverOffset = offsMin / 1.0e6f;
                    timeSyncData.serverLatency = latMin / 1.0e6f;
                    timeSyncData.cnt = 0;
                }
                timeSyncData.waiting = false;
            }
            break;
        }
        case prot::cmd::VERSION_CHECK:
        {
            if (flags & CMD_FLAG_RESP)
            {
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
                cmddes.value2b(major);
                cmddes.value2b(minor);
                cmddes.value2b(patch);
                if (major != version::MAJOR)
                {
                    LG_E("Cannot connect to server. Version mismatch. Server: {}.{}.{}, Client: {}.{}.{}", major, minor, patch, version::MAJOR, version::MINOR, version::PATCH);
                    return;
                }
                else
                {
                    if (minor != version::MINOR || patch != version::PATCH)
                    {
                        LG_W("Version mismatch. Server: {}.{}.{}, Client: {}.{}.{}", major, minor, patch, version::MAJOR, version::MINOR, version::PATCH);
                    }
                    LG_I("Version check successful");
                    authenticate();
                }
            }
            break;
        }
        case prot::cmd::AUTHENTICATE:
        {
            if (flags & CMD_FLAG_RESP)
            {
                LG_I("Authentication successful");
                gameState = ClientGameState::Authenticated;
            }
            break;
        }
        case prot::cmd::WORLD_INFO:
        {
            if (flags & CMD_FLAG_RESP)
            {
                def::WorldShape worldShape;
                cmddes.object(worldShape);
                world.createFromServer(worldShape);
            }
            break;
        }
        case prot::cmd::CONSOLE_CMD:
        {
            if (flags & CMD_FLAG_RESP)
            {
                std::string str;
                cmddes.text1b(str, len);
                LG_I("Console cmd response: {}", str);
                userInterface->addSystemMessage(str);
            }
            break;
        }
        default:
            break;
    }

    // Callbacks for custom commands...

    // Check exchange sequence progress
    if (gameState == ClientGameState::LoadWorld && !loadWorldSequence.done())
    {
        loadWorldSequence.advance(sendQueue, cmd, result);
    }
}

void Model::drawDebug(gfx::RenderEngine& renderer, float zoom)
{
    world.drawDebug(renderer, zoom);
}

void Model::sendCmdToServer(const std::string& command)
{
    CMDAT_PREP(net::SendType::TCP, prot::cmd::CONSOLE_CMD, 0)
    cmdser.text1b(command, command.size());
    CMDAT_FIN()
    sendQueue.enqueue(cmdData);
}

void Model::checkVersion(const net::ModelClientInfo& clientInfo)
{
    this->clientInfo = clientInfo;
    CMDAT_PREP(net::SendType::TCP, prot::cmd::VERSION_CHECK, 0)
    cmdser.value2b(version::MAJOR);
    cmdser.value2b(version::MINOR);
    cmdser.value2b(version::PATCH);
    CMDAT_FIN()
    sendQueue.enqueue(cmdData);
    gameState = ClientGameState::VersionCheck;
}

void Model::authenticate()
{
    LG_I("Authenticating with server...");
    gameState = ClientGameState::Authenticating;
    CMDAT_PREP(net::SendType::TCP, prot::cmd::AUTHENTICATE, 0)
    cmdser.value2b(version::MAJOR);
    cmdser.value2b(version::MINOR);
    cmdser.value2b(version::PATCH);
    cmdser.text1b(clientInfo.token, 16);
    cmdser.value2b((uint16_t)clientInfo.udpPortCli);
    CMDAT_FIN()
    sendQueue.enqueue(cmdData);
}

void Model::disconnectFromServer()
{
    switch (gameState)
    {
        case ClientGameState::Authenticating:
            LG_W("Authentication refused");
            gameState = ClientGameState::MainMenu;
            break;
        case ClientGameState::GameLoop:
            LG_W("Disconnecting from server");
            gameState = ClientGameState::MainMenu;
            break;
        default:
            break;
    }
}

}  // namespace sphyc