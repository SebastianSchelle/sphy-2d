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

    slowDumpComponents.push_back(CompClientDump(
        name,
        [this, name](const net::ClientInfo* clientInfo,
                     ecs::PtrHandle* ptrHandle)
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

                // Update entity count or rewind if no entities in sector
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
        }));
}

template <typename Component> void Engine::registerActiveSectorDumpComponent()
{
    const std::string name = Component::NAME;
    for (auto& component : activeSectorUpdates)
    {
        if (component.componentName == name)
        {
            return;
        }
    }

    activeSectorUpdates.push_back(CompActiveSectorUpdate(
        name,
        [this, name](const net::ClientInfo* clientInfo,
                     uint32_t sectorId,
                     ecs::PtrHandle* ptrHandle)
        {
            prot::MsgComposer mcomp(net::SendType::UDP,
                                    clientInfo->udpEndpoint);
            mcomp.startCommand(prot::cmd::SLOW_DUMP, 0);
            mcomp.ser->value4b(hashConst(name.c_str()));

            uint16_t entityCntMessage = 0;
            world::Sector* sector = ptrHandle->world->getSector(sectorId);

            size_t rewindPos = mcomp.ser->adapter().currentWritePos();
            mcomp.ser->value4b(sector->getId());
            size_t cntPos = mcomp.ser->adapter().currentWritePos();
            mcomp.ser->value2b((uint16_t)0);

            // Go through entities in sector and append component if it
            // exists
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
                            mcomp.ser->value2b(entityCntMessage);
                            mcomp.ser->adapter().currentWritePos(sectorEndPos);
                            mcomp.execute(sendQueue);
                            mcomp.resetData();
                            // Write message and sector header
                            mcomp.startCommand(prot::cmd::ACTIVE_SECTOR_UPDATE,
                                               0);
                            mcomp.ser->value4b(hashConst(name.c_str()));
                            rewindPos = mcomp.ser->adapter().currentWritePos();
                            mcomp.ser->value4b(sector->getId());
                            cntPos = mcomp.ser->adapter().currentWritePos();
                            mcomp.ser->value2b((uint16_t)0);
                            entityCntMessage = 0;
                        }
                    }
                }
            }
            if (entityCntMessage != 0)
            {
                size_t sectorEndPos = mcomp.ser->adapter().currentWritePos();
                mcomp.ser->adapter().currentWritePos(cntPos);
                mcomp.ser->value2b(entityCntMessage);
                mcomp.ser->adapter().currentWritePos(sectorEndPos);
                mcomp.execute(sendQueue);
            }
        }));
}

}  // namespace sphys

#endif