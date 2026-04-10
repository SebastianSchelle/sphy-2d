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
            int sectorIdx = 0;
            const vector<ecs::EntityId>* entityIds = nullptr;
            int entIdx = 0;

            auto moreSlowDumpWork = [&]() -> bool
            {
                const int sectorCount = ptrHandle->world->getSectorCount();
                if (sectorIdx >= sectorCount)
                {
                    return false;
                }
                if (entityIds != nullptr
                    && static_cast<size_t>(entIdx) < entityIds->size())
                {
                    return true;
                }
                return sectorIdx < sectorCount;
            };

            while (moreSlowDumpWork())
            {
                auto writer =
                    [this,
                     name,
                     ptrHandle,
                     &sectorIdx,
                     &entityIds,
                     &entIdx](bitsery::Serializer<OutputAdapter>& cmdser) -> bool
                {
                    uint16_t writtenEntities = 0;
                    size_t initialWritePos = cmdser.adapter().currentWritePos();
                    cmdser.value4b(hashConst(name.c_str()));

                    const int sectorCount = ptrHandle->world->getSectorCount();
                    while (sectorIdx < sectorCount)
                    {
                        world::Sector* sector =
                            ptrHandle->world->getSector(sectorIdx);
                        entityIds = &sector->getEntityIds();

                        if (static_cast<size_t>(entIdx) >= entityIds->size())
                        {
                            sectorIdx++;
                            entIdx = 0;
                            entityIds = nullptr;
                            continue;
                        }

                        if (cmdser.adapter().currentWritePos() + 6
                            > prot::kMaxSerializedChunkBytes)
                        {
                            break;
                        }

                        size_t beforeSectorBlock =
                            cmdser.adapter().currentWritePos();
                        cmdser.value4b(sector->getId());
                        size_t cntPos = cmdser.adapter().currentWritePos();
                        uint16_t numEntities = 0;
                        cmdser.value2b(numEntities);

                        while (static_cast<size_t>(entIdx) < entityIds->size())
                        {
                            if (numEntities > 0
                                && cmdser.adapter().currentWritePos()
                                        + prot::kSerializedRecordReserveBytes
                                    > prot::kMaxSerializedChunkBytes)
                            {
                                break;
                            }
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
                                    cmdser.object(entityId);
                                    cmdser.object(*component);
                                    numEntities++;
                                    writtenEntities++;
                                }
                            }
                            entIdx++;
                        }

                        if (numEntities == 0)
                        {
                            cmdser.adapter().currentWritePos(beforeSectorBlock);
                        }
                        else
                        {
                            cmdser.adapter().currentWritePos(cntPos);
                            cmdser.value2b(numEntities);
                            cmdser.adapter().currentWritePos(
                                cmdser.adapter().writtenBytesCount());
                        }

                        if (static_cast<size_t>(entIdx) < entityIds->size())
                        {
                            break;
                        }

                        sectorIdx++;
                        entIdx = 0;
                        entityIds = nullptr;
                    }

                    if (writtenEntities == 0)
                    {
                        cmdser.adapter().currentWritePos(initialWritePos);
                        return false;
                    }
                    return true;
                };

                prot::writeMessageUdp(
                    sendQueue,
                    &clientInfo->udpEndpoint,
                    [writer](bitsery::Serializer<OutputAdapter>& cmdser) -> bool
                    {
                        return prot::writeCommand(
                            cmdser, prot::cmd::SLOW_DUMP, 0, writer);
                    },
                    false,
                    0);
            }
        }));
}

}  // namespace sphys

#endif