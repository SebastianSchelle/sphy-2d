#ifndef ENGINE_IMPL_HPP
#define ENGINE_IMPL_HPP

#include "engine.hpp"
#include "protocol.hpp"

namespace sphys
{

template <typename Component> void Engine::registerSlowDumpComponent()
{
    const std::string name = Component::NAME;
    for (auto& component : slowDumpComponents)
    {
        if (component.componentName == name)
        {
            return;
        }
    }
    slowDumpComponents.push_back(SlowDumpEntry(
        name,
        [this, name](const net::ClientInfo* clientInfo,
                     std::shared_ptr<ecs::PtrHandle> ptrHandle)
        {
            bool lastSectorEmpty = false;
            CMDAT_PREP(net::SendType::UDP, prot::cmd::SLOW_DUMP, 0)
            cmdser.value4b(hashConst(name.c_str()));
            for (int i = 0; i < ptrHandle->world->getSectorCount(); i++)
            {
                uint16_t numEntities = 0;
                world::Sector* sector = ptrHandle->world->getSector(i);
                cmdser.value4b(sector->getId());
                size_t cntPos = cmdser.adapter().writtenBytesCount();
                cmdser.value2b(numEntities);
                for (auto entity : sector->getEntities())
                {
                    auto component =
                        ptrHandle->registry->try_get<Component>(entity);
                    if (component)
                    {
                        cmdser.object(*component);
                        numEntities++;
                    }
                }
                cmdser.adapter().currentWritePos(cntPos);
                lastSectorEmpty = (numEntities == 0);
                if(!lastSectorEmpty)
                {
                    cmdser.value2b(numEntities);
                    cmdser.adapter().currentWritePos(cmdser.adapter().writtenBytesCount());
                }
                else
                {
                    cmdser.adapter().currentWritePos(cntPos - 4);
                }
            }
            CMDAT_FIN_REM_TAIL_BYTES(lastSectorEmpty ? 6 : 0)
            cmdData.udpEndpoint = clientInfo->udpEndpoint;
            sendQueue.enqueue(cmdData);
        }));
}

}  // namespace sphys

#endif