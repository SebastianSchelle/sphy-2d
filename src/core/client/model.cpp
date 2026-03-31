#include <exchange-sequence.hpp>
#include <model.hpp>
#include <protocol.hpp>
#include <world-def.hpp>
#include <ui/user-interface.hpp>

namespace sphyc
{

Model::Model(ui::UserInterface* userInterface) : userInterface(userInterface)
{
    loadWorldSequence.registerExchange(net::Exchange(
        prot::cmd::WORLD_INFO,
        [this]() {},
        [this]() {},
        [this](bitsery::Serializer<OutputAdapter>& ser) {}));
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
        case ClientGameState::Connected:
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
    LG_D("Attempting time sync");
    timeSyncData.t0 = tim::getCurrentTimeU();
    timeSyncData.waiting = true;
    CMDAT_PREP_TOKEN(net::SendType::UDP, prot::cmd::TIME_SYNC, 0)
    CMDAT_FIN_TOKEN()
    sendQueue.enqueue(cmdData);
}

void Model::modelLoopMenu(float dt) {}

void Model::modelLoopGame(float dt)
{
    static tim::Timepoint lastTSync = tim::getCurrentTimeU();
    tim::Timepoint now = tim::getCurrentTimeU();

    // Send some stuff to server
    /*CMDAT_PREP_TOKEN(net::SendType::UDP, prot::cmd::LOG, 0)
    std::string str = "Hello World!";
    cmdser.text1b(str, str.size());
    CMDAT_FIN_TOKEN()
    sendQueue.enqueue(cmdData);*/

    if (tim::durationU(lastTSync, now) > 2000000)
    {
        timeSync();
        lastTSync = now;
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
                timeSyncData.t1 = tim::getCurrentTimeU();
                timeSyncData.waiting = false;
                LG_I("Time sync successful");
            }
            break;
        }
        case prot::cmd::CONNECT:
        {
            if (flags & CMD_FLAG_RESP)
            {
                LG_I("Authentication successful");
                gameState = ClientGameState::Connected;
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

}  // namespace sphyc