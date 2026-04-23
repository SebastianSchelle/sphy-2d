#include "comp-phy.hpp"
#include <comp-ident.hpp>
#include <config-manager.hpp>
#include <ptr-handle.hpp>
#include <world.hpp>

const uint16_t def::WorldShape::VERSION;

namespace world
{

World::World() {}

World::~World() {}

bool World::createFromConfig(cfg::ConfigManager& config)
{
    worldShape.numSectorX = CFG_UINT(config, 10.0f, "world", "num-sector-x");
    worldShape.numSectorY = CFG_UINT(config, 10.0f, "world", "num-sector-y");
    worldShape.sectorSize = CFG_FLOAT(config, 5000.0f, "world", "sector-size");
    if (!initWorld())
    {
        LG_E("World initialization failed");
        return false;
    }
    if (!initSectors(false))
    {
        LG_E("Sectors initialization failed");
        return false;
    }
    dirty = true;
    return true;
}

bool World::createFromSave(cfg::ConfigManager& config,
                           const std::string& savedir)
{
    std::string worldSaveFld = savedir + "/save-data/world";
    std::string worldFilePath = worldSaveFld + "/world." + GAME_NAME + ".sav";
    if (!LOAD_OBJ(worldFilePath,
                  [this](uint32_t typeId,
                         uint16_t version,
                         bitsery::Deserializer<InputAdapter>& des_)
                  { return loadWorldProcessData(typeId, version, des_); }))
    {
        LG_E("Failed to load world from save");
        return false;
    }
    if (!initWorld())
    {
        LG_E("World initialization failed");
        return false;
    }
    if (!initSectors(true))
    {
        LG_E("Sectors initialization failed");
        return false;
    }
    return true;
}

bool World::createFromServer(const def::WorldShape& worldShape)
{
    this->worldShape = worldShape;
    if (!initWorld())
    {
        LG_E("World initialization failed");
        return false;
    }
    if (!initSectors(false))
    {
        LG_E("Sectors initialization failed");
        return false;
    }
    return true;
}

bool World::loadWorldProcessData(uint32_t typeId,
                                 uint16_t version,
                                 bitsery::Deserializer<InputAdapter>& des_)
{
    switch (typeId)
    {
        case TYPE_ID(def::WorldShape):
            if (version != def::WorldShape::VERSION)
            {
                LG_E("WorldShape version mismatch");
                return false;
            }
            des_.object(worldShape);
            break;
        default:
            LG_W("Unknown type id: {}", typeId);
            break;
    }
    return true;
}

bool World::initWorld()
{
    if (worldShape.numSectorX == 0 || worldShape.numSectorY == 0
        || worldShape.sectorSize <= 0)
    {
        LG_E("Invalid world shape. World initialization failed");
    }
    halfSectorSize = worldShape.sectorSize / 2.0f;
    sectors.init(worldShape.numSectorX, worldShape.numSectorY);
    LG_I("World initialized with {} sectors", sectors.getSize());
    return true;
}

bool World::initSectors(bool fromSave)
{
    if (0 && fromSave)
    {
        LG_E("Not implemented");
    }
    else
    {
        for (int i = 0; i < worldShape.numSectorX; i++)
        {
            for (int j = 0; j < worldShape.numSectorY; j++)
            {
                uint32_t sectorId = sectors.coordToIdx(i, j);
                Sector* neighbors[8];
                for (int k = 0; k < 8; k++)
                {
                    neighbors[k] = getNeighboringSector(
                        i, j, static_cast<def::Direction>(k));
                }
                sectors.at(i, j)->init(
                    i, j, worldShape.sectorSize, sectorId, neighbors);
            }
        }
    }
    return true;
}

bool World::saveWorld(const std::string& savedir)
{
    if (dirty)
    {
        std::string worldSaveFld = savedir + "/save-data/world";
        LG_I("Save world to folder: {}", worldSaveFld);
        if (!fs::exists(worldSaveFld))
        {
            fs::create_directories(worldSaveFld);
        }
        std::string worldFilePath =
            worldSaveFld + "/world." + GAME_NAME + ".sav";
        SAVE_OBJ_PREP();
        SAVE_SER_OBJECT(ser_, worldShape, def::WorldShape);
        SAVE_OBJ_FIN(worldFilePath);
    }
    for (int i = 0; i < worldShape.numSectorX; i++)
    {
        for (int j = 0; j < worldShape.numSectorY; j++)
        {
            sectors.at(i, j)->saveSector(savedir);
        }
    }
    return true;
}

void World::update(float dt, ecs::PtrHandle* ptrHandle)
{
    // todo: add multithreading
    /*
        thoughts:
            - perf result for phyThrust shows the following:
                    43.01 │       movss   0x18(%rdx),%xmm1 ▒ 25.14 │       movss
       0x10(%rdx),%xmm0 looks like high memory access latency?
            - assign sectors to the same thread each time to reduce cache misses
            - Use worker pool and assign sector to same thread if thread is not
       overloaded
            - store some lastThreadId for each sector and assign to same thread
       if possible
            - per-sector SoA vel/acc/mass/... (might be overkill)
            - Put physics/collission completely out of entt and into sector?
            -
    */
    for (uint32_t sectorId = 0; sectorId < sectors.getSize(); sectorId++)
    {
        ptrHandle->workDistributor->addWork(
            [this, sectorId, dt, ph = ptrHandle]()
            { sectors.at(sectorId)->update(dt, ph); },
            sectorId % ptrHandle->workDistributor->getThreadCount());
    }
    ptrHandle->workDistributor->awaken();
    ptrHandle->workDistributor->waitForEmptyQueues();
    ptrHandle->workDistributor->suspend();
    handleSectorMoveRequests(ptrHandle);
}

bool World::getNeighboringSectorId(uint32_t sectorId,
                                   def::Direction dir,
                                   def::SectorPos& newPos)
{
    auto [sectorX, sectorY] = idToSectorCoords(sectorId);
    return getNeighboringSectorId(sectorX, sectorY, dir, newPos);
}

bool World::getNeighboringSectorId(uint32_t sectorX,
                                   uint32_t sectorY,
                                   def::Direction dir,
                                   def::SectorPos& newPos)
{
    switch (dir)
    {
        case def::Direction::N:
            if (sectorY == 0)
            {
                return false;
            }
            newPos.x = sectorX;
            newPos.y = sectorY - 1;
            return true;
        case def::Direction::S:
            if (sectorY == worldShape.numSectorY - 1)
            {
                return false;
            }
            newPos.x = sectorX;
            newPos.y = sectorY + 1;
            return true;
        case def::Direction::W:
            if (sectorX == 0)
            {
                return false;
            }
            newPos.x = sectorX - 1;
            newPos.y = sectorY;
            return true;
        case def::Direction::E:
            if (sectorX == worldShape.numSectorX - 1)
            {
                return false;
            }
            newPos.x = sectorX + 1;
            newPos.y = sectorY;
            return true;
        case def::Direction::NW:
            if (sectorX == 0 || sectorY == 0)
            {
                return false;
            }
            newPos.x = sectorX - 1;
            newPos.y = sectorY - 1;
            return true;
        case def::Direction::NE:
            if (sectorX == worldShape.numSectorX - 1 || sectorY == 0)
            {
                return false;
            }
            newPos.x = sectorX + 1;
            newPos.y = sectorY - 1;
            return true;
        case def::Direction::SW:
            if (sectorX == 0 || sectorY == worldShape.numSectorY - 1)
            {
                return false;
            }
            newPos.x = sectorX - 1;
            newPos.y = sectorY + 1;
            return true;
        case def::Direction::SE:
            if (sectorX == worldShape.numSectorX - 1
                || sectorY == worldShape.numSectorY - 1)
            {
                return false;
            }
            newPos.x = sectorX + 1;
            newPos.y = sectorY + 1;
            return true;
        default:
            return false;
    }
    return true;
}

Sector* World::getNeighboringSector(uint32_t x, uint32_t y, def::Direction dir)
{
    def::SectorPos newPos;
    if (!getNeighboringSectorId(x, y, dir, newPos))
    {
        return nullptr;
    }
    return sectors.at(newPos.x, newPos.y);
}

bool World::moveEntityTo(ecs::PtrHandle* ptrHandle,
                         ecs::EntityId entityId,
                         uint32_t sectorId,
                         glm::vec2 position,
                         float rotation)
{
    auto reg = ptrHandle->registry;
    entt::entity entity = ptrHandle->ecs->getEntity(entityId);

    ecs::Transform& transform = reg->get<ecs::Transform>(entity);
    transform.pos = position;
    transform.rot = rotation;

    switchSector(ptrHandle, entityId, sectorId);
    return true;
}

bool World::switchSector(ecs::PtrHandle* ptrHandle,
                         ecs::EntityId entityId,
                         uint32_t newSectorId)
{
    if (!ptrHandle->ecs->validId(entityId))
    {
        LG_W("Entity not valid: {}", entityId);
        return false;
    }
    Sector* newSector = sectors.at(newSectorId);
    if (newSector == nullptr)
    {
        LG_W("Sector not found: {}", newSectorId);
        return false;
    }

    auto reg = ptrHandle->registry;
    entt::entity entity = ptrHandle->ecs->getEntity(entityId);
    auto sector = reg->try_get<ecs::SectorId>(entity);
    if (sector && sector->id != INVALID_SECTOR_ID && sector->id != newSectorId)
    {
        Sector* oldSector = sectors.at(sector->id);
        if (oldSector)
        {
            oldSector->removeEntity(ptrHandle, entityId);
        }
    }
    newSector->addEntity(ptrHandle, entityId);
    return true;
}

Sector* World::getSector(uint32_t sectorId)
{
    return sectors.at(sectorId);
}

def::SectorPos World::idToSectorCoords(uint32_t sectorId) const
{
    return def::SectorPos{sectorId % worldShape.numSectorX,
                          sectorId / worldShape.numSectorY};
}

uint32_t World::sectorCoordsToId(uint32_t sectorX, uint32_t sectorY) const
{
    return sectorX + sectorY * worldShape.numSectorX;
}

vec2 World::getWorldPosSectorOffset(uint32_t sectorX,
                                    uint32_t sectorY,
                                    int32_t sectorOffsetX,
                                    int32_t sectorOffsetY) const
{
    return vec2((int32_t)sectorX - sectorOffsetX,
                (int32_t)sectorY - sectorOffsetY)
           * worldShape.sectorSize;
}

vec2 World::getWorldPosSectorOffset(uint32_t sectorId,
                                    int32_t sectorOffsetX,
                                    int32_t sectorOffsetY) const
{
    auto [sectorX, sectorY] = idToSectorCoords(sectorId);
    return getWorldPosSectorOffset(
        sectorX, sectorY, sectorOffsetX, sectorOffsetY);
}

void World::checkSectorSwitchAfterMove(ecs::EntityId entityId,
                                       entt::entity entity,
                                       ecs::SectorId* sectorId,
                                       ecs::Transform* transform,
                                       ecs::PtrHandle* ptrHandle)
{
    vec2& pos = transform->pos;
    def::Direction dir = def::Direction::NONE;
    def::SectorPos newPos;
    if (pos.x < -halfSectorSize)
    {
        if (pos.y < -halfSectorSize)
        {
            dir = def::Direction::NW;
            if (!getNeighboringSectorId(sectorId->id, dir, newPos))
            {
                if (getNeighboringSectorId(
                        sectorId->id, def::Direction::N, newPos))
                {
                    pos.x = -halfSectorSize;
                    dir = def::Direction::N;
                }
                else if (getNeighboringSectorId(
                             sectorId->id, def::Direction::W, newPos))
                {
                    pos.y = -halfSectorSize;
                    dir = def::Direction::W;
                }
            }
        }
        else if (pos.y > halfSectorSize)
        {
            dir = def::Direction::SW;
            if (!getNeighboringSectorId(sectorId->id, dir, newPos))
            {
                if (getNeighboringSectorId(
                        sectorId->id, def::Direction::S, newPos))
                {
                    pos.x = -halfSectorSize;
                    dir = def::Direction::S;
                }
                else if (getNeighboringSectorId(
                             sectorId->id, def::Direction::W, newPos))
                {
                    pos.y = halfSectorSize;
                    dir = def::Direction::W;
                }
            }
        }
        else
        {
            dir = def::Direction::W;
        }
    }
    else if (pos.x > halfSectorSize)
    {
        if (pos.y < -halfSectorSize)
        {
            dir = def::Direction::NE;
            if (!getNeighboringSectorId(sectorId->id, dir, newPos))
            {
                if (getNeighboringSectorId(
                        sectorId->id, def::Direction::N, newPos))
                {
                    pos.x = halfSectorSize;
                    dir = def::Direction::N;
                }
                else if (getNeighboringSectorId(
                             sectorId->id, def::Direction::E, newPos))
                {
                    pos.y = -halfSectorSize;
                    dir = def::Direction::E;
                }
            }
        }
        else if (pos.y > halfSectorSize)
        {
            dir = def::Direction::SE;
            if (!getNeighboringSectorId(sectorId->id, dir, newPos))
            {
                if (getNeighboringSectorId(
                        sectorId->id, def::Direction::S, newPos))
                {
                    pos.x = halfSectorSize;
                    dir = def::Direction::S;
                }
                else if (getNeighboringSectorId(
                             sectorId->id, def::Direction::E, newPos))
                {
                    pos.y = halfSectorSize;
                    dir = def::Direction::E;
                }
            }
        }
        else
        {
            dir = def::Direction::E;
        }
    }
    else if (pos.y < -halfSectorSize)
    {
        dir = def::Direction::N;
    }
    else if (pos.y > halfSectorSize)
    {
        dir = def::Direction::S;
    }
    if (dir != def::Direction::NONE)
    {
        if (getNeighboringSectorId(sectorId->id, dir, newPos))
        {
            uint32_t newSectorId = sectorCoordsToId(newPos.x, newPos.y);
            addSectorMoveRequest(ptrHandle, entityId, newSectorId);
            switch (dir)
            {
                case def::Direction::N:
                    pos.y = pos.y + worldShape.sectorSize;
                    break;
                case def::Direction::S:
                    pos.y = pos.y - worldShape.sectorSize;
                    break;
                case def::Direction::W:
                    pos.x = pos.x + worldShape.sectorSize;
                    break;
                case def::Direction::E:
                    pos.x = pos.x - worldShape.sectorSize;
                    break;
                case def::Direction::NW:
                    pos.x = pos.x + worldShape.sectorSize;
                    pos.y = pos.y + worldShape.sectorSize;
                    break;
                case def::Direction::NE:
                    pos.x = pos.x - worldShape.sectorSize;
                    pos.y = pos.y + worldShape.sectorSize;
                    break;
                case def::Direction::SW:
                    pos.x = pos.x + worldShape.sectorSize;
                    pos.y = pos.y - worldShape.sectorSize;
                    break;
                case def::Direction::SE:
                    pos.x = pos.x - worldShape.sectorSize;
                    pos.y = pos.y - worldShape.sectorSize;
                    break;
                default:
                    break;
            }
        }
        else
        {
            switch (dir)
            {
                case def::Direction::N:
                    pos.y = -halfSectorSize;
                    break;
                case def::Direction::S:
                    pos.y = halfSectorSize;
                    break;
                case def::Direction::W:
                    pos.x = -halfSectorSize;
                    break;
                case def::Direction::E:
                    pos.x = halfSectorSize;
                    break;
                case def::Direction::NW:
                    pos.x = -halfSectorSize;
                    pos.y = -halfSectorSize;
                    break;
                case def::Direction::NE:
                    pos.x = halfSectorSize;
                    pos.y = -halfSectorSize;
                    break;
                case def::Direction::SW:
                    pos.x = -halfSectorSize;
                    pos.y = halfSectorSize;
                    break;
                case def::Direction::SE:
                    pos.x = halfSectorSize;
                    pos.y = halfSectorSize;
                    break;
                default:
                    break;
            }
        }
    }
}

void World::addSectorMoveRequest(ecs::PtrHandle* ptrHandle,
                                 ecs::EntityId entityId,
                                 uint32_t newSectorId)
{
    sectorMoveRequests.enqueue(
        SectorMoveRequest{entityId, newSectorId});
}

void World::handleSectorMoveRequests(ecs::PtrHandle* ptrHandle)
{
    SectorMoveRequest request;
    while (sectorMoveRequests.try_dequeue(request))
    {
        if (!switchSector(
                ptrHandle, request.entityId, request.newSectorId))
        {
            LG_E("Failed to switch sector for entity: {} to sector: {}",
                 request.entityId,
                 request.newSectorId);
        }
    }
}

bool World::sectorIntersectsRect(uint32_t sectorId, const Rect& rect) const
{
    auto [sectorX, sectorY] = idToSectorCoords(sectorId);
    return sectorIntersectsRect(sectorX, sectorY, rect);
}

bool World::sectorIntersectsRect(int32_t sectorX,
                                 int32_t sectorY,
                                 const glm::vec4& rect) const
{
    Rect sectorRect =
        Rect((float)sectorX * worldShape.sectorSize - halfSectorSize,
             (float)sectorY * worldShape.sectorSize - halfSectorSize,
             (float)sectorX * worldShape.sectorSize + halfSectorSize,
             (float)sectorY * worldShape.sectorSize + halfSectorSize);
    return smath::intersectsRect(sectorRect, rect);
}

#ifdef CLIENT
void World::drawDebug(gfx::RenderEngine& renderer, float zoom)
{
    for (int i = 0; i < worldShape.numSectorX; i++)
    {
        for (int j = 0; j < worldShape.numSectorY; j++)
        {
            sectors.at(i, j)->drawDebug(renderer, zoom);
        }
    }
}

void World::drawTacticalMap(gfx::RenderEngine& renderer,
                            const Rect& viewRect,
                            float zoom)
{
}

void World::drawStrategicMap(gfx::RenderEngine& renderer,
                             const Rect& viewRect,
                             float zoom)
{
    // todo: Could just get min/max sector coords from viewRect and then loop
    // through those
    for (int i = 0; i < worldShape.numSectorX; i++)
    {
        for (int j = 0; j < worldShape.numSectorY; j++)
        {
            // todo: this seems wrong
            // if (sectorIntersectsRect(i - renderer.getSectorOffsetX(),
            //                          j - renderer.getSectorOffsetY(),
            //                          viewRect))
            {
                sectors.at(i, j)->drawStrategicMap(renderer, viewRect, zoom);
            }
        }
    }
}

void World::drawThirdPerson(gfx::RenderEngine& renderer,
                            const Rect& viewRect,
                            float zoom)
{
}
#endif

}  // namespace world