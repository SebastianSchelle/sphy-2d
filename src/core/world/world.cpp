#include <config-manager.hpp>
#include <world.hpp>

const uint16_t def::WorldShape::VERSION;

namespace world
{

World::World() {}

World::~World() {}

bool World::createFromConfig(cfg::ConfigManager& config)
{
    worldShape.numSectorX = CFG_UINT(config, "world", "num-sector-x");
    worldShape.numSectorY = CFG_UINT(config, "world", "num-sector-y");
    worldShape.sectorSize = CFG_FLOAT(config, "world", "sector-size");
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

void World::update(float dt, std::shared_ptr<ecs::PtrHandle> ptrHandle)
{
    // todo: add multithreading
    for (int i = 0; i < worldShape.numSectorX; i++)
    {
        for (int j = 0; j < worldShape.numSectorY; j++)
        {
            sectors.at(i, j)->update(dt, ptrHandle);
        }
    }
}

Sector* World::getNeighboringSector(uint32_t x, uint32_t y, def::Direction dir)
{
    switch (dir)
    {
        case def::Direction::N:
            if (y == 0)
            {
                return nullptr;
            }
            return sectors.at(x, y - 1);
        case def::Direction::S:
            if (y == worldShape.numSectorY - 1)
            {
                return nullptr;
            }
            return sectors.at(x, y + 1);
        case def::Direction::W:
            if (x == 0)
            {
                return nullptr;
            }
            return sectors.at(x - 1, y);
        case def::Direction::E:
            if (x == worldShape.numSectorX - 1)
            {
                return nullptr;
            }
            return sectors.at(x + 1, y);
        case def::Direction::NW:
            if (x == 0 || y == 0)
            {
                return nullptr;
            }
            return sectors.at(x - 1, y - 1);
        case def::Direction::NE:
            if (x == worldShape.numSectorX - 1 || y == 0)
            {
                return nullptr;
            }
            return sectors.at(x + 1, y - 1);
        case def::Direction::SW:
            if (x == 0 || y == worldShape.numSectorY - 1)
            {
                return nullptr;
            }
            return sectors.at(x - 1, y + 1);
        case def::Direction::SE:
            if (x == worldShape.numSectorX - 1
                || y == worldShape.numSectorY - 1)
            {
                return nullptr;
            }
            return sectors.at(x + 1, y + 1);
        default:
            return nullptr;
    }
}

}  // namespace world