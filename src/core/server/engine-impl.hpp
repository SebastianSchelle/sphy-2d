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

    // todo: start new message if over max udp size

    slowDumpComponents.push_back(SlowDumpEntry(
        name,
        [this, name](const net::ClientInfo* clientInfo,
                     std::shared_ptr<ecs::PtrHandle> ptrHandle)
        {
            bool lastSectorEmpty = false;
            int sectorIdx = 0;
            const vector<ecs::EntityId>* entityIds = nullptr;
            int entIdx = 0;
            do
            {
                auto writer =
                    [this,
                     name,
                     ptrHandle,
                     &lastSectorEmpty,
                     &sectorIdx,
                     &entityIds,
                     &entIdx](bitsery::Serializer<OutputAdapter>& cmdser)
                {
                    bool msgFull = false;
                    uint16_t numEntities = 0;

                    cmdser.value4b(hashConst(name.c_str()));

                    while (sectorIdx < ptrHandle->world->getSectorCount())
                    {
                        // Write sector index and dummy entity cnt
                        world::Sector* sector =
                            ptrHandle->world->getSector(sectorIdx);
                        cmdser.value4b(sector->getId());
                        size_t cntPos = cmdser.adapter().writtenBytesCount();
                        cmdser.value2b(numEntities);
                        entityIds = &sector->getEntityIds();
                        auto entities = &sector->getEntities();
                        while (entIdx < entityIds->size())
                        {
                            auto entityId = (*entityIds)[entIdx];
                            entt::entity entity =
                                ptrHandle->ecs->getEntity(entityId);
                            if (entity != entt::null)
                            {
                                auto component =
                                    ptrHandle->registry->try_get<Component>(
                                        entity);
                                if (component)
                                {
                                    cmdser.value4b(entityId.index);
                                    cmdser.value2b(entityId.generation);
                                    cmdser.object(*component);
                                    numEntities++;
                                }
                            }
                            entIdx++;
                        }
                        cmdser.adapter().currentWritePos(cntPos);
                        lastSectorEmpty = (numEntities == 0);
                        if (!lastSectorEmpty)
                        {
                            cmdser.value2b(numEntities);
                            cmdser.adapter().currentWritePos(
                                cmdser.adapter().writtenBytesCount());
                        }
                        else
                        {
                            cmdser.adapter().currentWritePos(cntPos - 4);
                        }
                        entIdx = 0;
                        numEntities = 0;
                        sectorIdx++;
                    }
                };

                prot::writeMessageUdp(
                    sendQueue,
                    &clientInfo->udpEndpoint,
                    [this,
                     name,
                     ptrHandle,
                     &lastSectorEmpty,
                     &sectorIdx,
                     &entityIds,
                     &entIdx,
                     writer](bitsery::Serializer<OutputAdapter>& cmdser)
                    {
                        prot::writeCommand(
                            cmdser, prot::cmd::SLOW_DUMP, 0, writer);
                    },
                    false,
                    lastSectorEmpty ? 6 : 0);
            } while (sectorIdx < ptrHandle->world->getSectorCount()
                     || entIdx < entityIds->size());
        }));
}

}  // namespace sphys

#endif