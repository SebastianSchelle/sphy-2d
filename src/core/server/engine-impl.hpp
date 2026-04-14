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
            prot::MsgComposer mcomp(net::SendType::UDP,
                                    clientInfo->udpEndpoint);
            mcomp.startCommand(prot::cmd::SLOW_DUMP, 0);
            mcomp.ser->value4b(hashConst(name.c_str()));

            uint16_t entityCntMessage = 0;
            const int sectorCount = ptrHandle->world->getSectorCount();
            for (int sectorIdx = 0; sectorIdx < sectorCount; ++sectorIdx)
            {
                world::Sector* sector = ptrHandle->world->getSector(sectorIdx);
                size_t rewindPos = mcomp.ser->adapter().currentWritePos();
                mcomp.ser->value4b(sector->getId());
                size_t cntPos = mcomp.ser->adapter().currentWritePos();
                mcomp.ser->value2b((uint16_t)0);

                // Go through entities in sector and append component if it
                // exists
                uint16_t entityCntSector = 0;
                auto& entityIds = sector->getEntityIds();
                for (int entIdx = 0; entIdx < entityIds.size(); ++entIdx)
                {
                    auto& entityId = entityIds[entIdx];
                    entt::entity entity = ptrHandle->ecs->getEntity(entityId);

                    if (entity != entt::null)
                    {
                        auto component =
                            ptrHandle->registry->try_get<Component>(entity);
                        if (component)
                        {
                            mcomp.ser->object(entityId);
                            mcomp.ser->object(*component);
                            entityCntSector++;
                            entityCntMessage++;

                            // Flush if packet limit is reached
                            if (mcomp.ser->adapter().currentWritePos() + 6
                                > prot::kMaxSerializedChunkBytes)
                            {
                                // Always update entity count in sector, min.
                                // one entity
                                size_t sectorEndPos =
                                    mcomp.ser->adapter().currentWritePos();
                                mcomp.ser->adapter().currentWritePos(cntPos);
                                mcomp.ser->value2b(entityCntSector);
                                mcomp.ser->adapter().currentWritePos(
                                    sectorEndPos);
                                mcomp.execute(sendQueue);
                                mcomp.resetData();
                                // Write message and sector header
                                mcomp.startCommand(prot::cmd::SLOW_DUMP, 0);
                                mcomp.ser->value4b(hashConst(name.c_str()));
                                rewindPos =
                                    mcomp.ser->adapter().currentWritePos();
                                mcomp.ser->value4b(sector->getId());
                                cntPos = mcomp.ser->adapter().currentWritePos();
                                mcomp.ser->value2b((uint16_t)0);
                                entityCntSector = 0;
                                entityCntMessage = 0;
                            }
                        }
                    }
                }

                // Update entiy count or rewind if no entities in sector
                if (entityCntSector > 0)
                {
                    size_t sectorEndPos =
                        mcomp.ser->adapter().currentWritePos();
                    mcomp.ser->adapter().currentWritePos(cntPos);
                    mcomp.ser->value2b(entityCntSector);
                    mcomp.ser->adapter().currentWritePos(sectorEndPos);
                }
                else
                {
                    mcomp.ser->adapter().currentWritePos(rewindPos);
                }
            }

            if (entityCntMessage != 0)
            {
                mcomp.execute(sendQueue);
            }

            // int sectorIdx = 0;
            // const vector<ecs::EntityId>* entityIds = nullptr;
            // int entIdx = 0;

            // auto moreSlowDumpWork = [&]() -> bool
            // {
            //     const int sectorCount = ptrHandle->world->getSectorCount();
            //     if (sectorIdx >= sectorCount)
            //     {
            //         return false;
            //     }
            //     if (entityIds != nullptr
            //         && static_cast<size_t>(entIdx) < entityIds->size())
            //     {
            //         return true;
            //     }
            //     return sectorIdx < sectorCount;
            // };

            // while (moreSlowDumpWork())
            // {
            //     auto writer =
            //         [this, name, ptrHandle, &sectorIdx, &entityIds, &entIdx](
            //             bitsery::Serializer<OutputAdapter>& cmdser) -> bool
            //     {
            //         uint16_t writtenEntities = 0;
            //         size_t initialWritePos =
            //         cmdser.adapter().currentWritePos();
            //         cmdser.value4b(hashConst(name.c_str()));

            //         const int sectorCount =
            //         ptrHandle->world->getSectorCount(); while (sectorIdx <
            //         sectorCount)
            //         {
            //             world::Sector* sector =
            //                 ptrHandle->world->getSector(sectorIdx);
            //             entityIds = &sector->getEntityIds();

            //             if (static_cast<size_t>(entIdx) >= entityIds->size())
            //             {
            //                 sectorIdx++;
            //                 entIdx = 0;
            //                 entityIds = nullptr;
            //                 continue;
            //             }

            //             if (cmdser.adapter().currentWritePos() + 6
            //                 > prot::kMaxSerializedChunkBytes)
            //             {
            //                 break;
            //             }

            //             size_t beforeSectorBlock =
            //                 cmdser.adapter().currentWritePos();
            //             cmdser.value4b(sector->getId());
            //             size_t cntPos = cmdser.adapter().currentWritePos();
            //             uint16_t numEntities = 0;
            //             cmdser.value2b(numEntities);

            //             while (static_cast<size_t>(entIdx) <
            //             entityIds->size())
            //             {
            //                 if (numEntities > 0
            //                     && cmdser.adapter().currentWritePos()
            //                                +
            //                                prot::kSerializedRecordReserveBytes
            //                            > prot::kMaxSerializedChunkBytes)
            //                 {
            //                     break;
            //                 }
            //                 auto entityId = (*entityIds)[entIdx];
            //                 entt::entity entity =
            //                     ptrHandle->ecs->getEntity(entityId);
            //                 if (entity != entt::null)
            //                 {
            //                     auto component =
            //                         ptrHandle->registry->try_get<Component>(
            //                             entity);
            //                     if (component)
            //                     {
            //                         cmdser.object(entityId);
            //                         cmdser.object(*component);
            //                         numEntities++;
            //                         writtenEntities++;
            //                     }
            //                 }
            //                 entIdx++;
            //             }

            //             if (numEntities == 0)
            //             {
            //                 cmdser.adapter().currentWritePos(beforeSectorBlock);
            //             }
            //             else
            //             {
            //                 cmdser.adapter().currentWritePos(cntPos);
            //                 cmdser.value2b(numEntities);
            //                 cmdser.adapter().currentWritePos(
            //                     cmdser.adapter().writtenBytesCount());
            //             }

            //             if (static_cast<size_t>(entIdx) < entityIds->size())
            //             {
            //                 break;
            //             }

            //             sectorIdx++;
            //             entIdx = 0;
            //             entityIds = nullptr;
            //         }

            //         if (writtenEntities == 0)
            //         {
            //             cmdser.adapter().currentWritePos(initialWritePos);
            //             return false;
            //         }
            //         return true;
            //     };

            //     prot::writeMessageUdp(
            //         sendQueue,
            //         &clientInfo->udpEndpoint,
            //         [writer](bitsery::Serializer<OutputAdapter>& cmdser) ->
            //         bool
            //         {
            //             return prot::writeCommand(
            //                 cmdser, prot::cmd::SLOW_DUMP, 0, writer);
            //         },
            //         false,
            //         0);
            // }
        }));
}

}  // namespace sphys

#endif