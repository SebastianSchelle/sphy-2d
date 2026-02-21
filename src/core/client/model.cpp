#include <model.hpp>
#include <protocol.hpp>

namespace sphyc
{

Model::Model() {}

Model::~Model()
{
}

void Model::modelLoop(float dt)
{
    net::CmdQueueData recQueueData;
    while (receiveQueue.try_dequeue(recQueueData))
    {
        parseCommand(recQueueData.data);
    }
    if (connectionState == GameState::DISCONNECTED)
    {
        modelLoopMenu(dt);
    }
    else if (connectionState == GameState::CONNECTED)
    {
        modelLoopGame(dt);
    }
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

void Model::modelLoopMenu(float dt)
{
}

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
                connectionState = GameState::CONNECTED;
            }
            break;
        }
        default:
            break;
    }
}

}  // namespace sphyc