#include <model.hpp>
#include <protocol.hpp>

namespace sphyc
{

Model::Model() {}

Model::~Model() {}

void Model::start()
{
    modelThread = std::thread([this]() { modelLoop(); });
}

void Model::modelLoop()
{
    while (true)
    {
        net::CmdQueueData recQueueData;
        while (receiveQueue.try_dequeue(recQueueData))
        {
            parseCommand(recQueueData.data);
        }
        if (connectionState == ConnectionState::DISCONNECTED)
        {
            modelLoopMenu();
        }
        else if (connectionState == ConnectionState::CONNECTED)
        {
            modelLoopGame();
        }
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

void Model::modelLoopMenu()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void Model::modelLoopGame()
{
    static tim::Timepoint lastTSync = tim::getCurrentTimeU();
    tim::Timepoint now = tim::getCurrentTimeU();

    // Send some stuff to server
    CMDAT_PREP_TOKEN(net::SendType::UDP, prot::cmd::LOG, 0)
    std::string str = "Hello World!";
    cmdser.text1b(str, str.size());
    CMDAT_FIN_TOKEN()
    sendQueue.enqueue(cmdData);

    if (tim::durationU(lastTSync, now) > 2000000)
    {
        // timeSync();
        lastTSync = now;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
            }
            break;
        }
        case prot::cmd::CONNECT:
        {
            if (flags & CMD_FLAG_RESP)
            {
                LG_I("Authentication successful");
                connectionState = ConnectionState::CONNECTED;
            }
            break;
        }
        default:
            break;
    }
}

}  // namespace sphyc