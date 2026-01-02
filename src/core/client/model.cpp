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
        CmdQueueData recQueueData;
        while (receiveQueue.try_dequeue(recQueueData))
        {
            parseCommand(recQueueData.data);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
        case prot::cmd::CMD_LOG:
        {
            std::string str;
            cmddes.text1b(str, len);
            LG_I("Received (len={}): {}", len, str);
            break;
        }
        default:
            break;
    }
}

}  // namespace sphyc