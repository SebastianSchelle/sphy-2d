#include <sector.hpp>

namespace world
{

Sector::Sector() {}

Sector::~Sector() {}

void Sector::init(int x,
                  int y,
                  float sectorSize,
                  uint32_t id,
                  Sector* neighbors[8])
{
    coordX = x;
    coordY = y;
    this->sectorSize = sectorSize;
    worldPosX = coordX * sectorSize + sectorSize / 2;
    worldPosY = coordY * sectorSize + sectorSize / 2;
    this->id = id;
    for (int i = 0; i < 8; i++)
    {
        this->neighbors[i] = neighbors[i];
    }
    dirty = true;
}

bool Sector::saveSector(const std::string& savedir)
{
    if (dirty)
    {
        std::string sectorSaveFld = savedir + "/save-data/world";
        std::string sectorFilePath = savedir + "/sector-" + std::to_string(id) + "." + GAME_NAME + ".sav";
        LG_I("Save sector to file: {}", sectorFilePath);
    }
    return true;
}

}  // namespace world