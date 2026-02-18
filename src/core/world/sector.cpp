#include <sector.hpp>

namespace world
{

Sector::Sector()
{
}

Sector::~Sector()
{
}

void Sector::init(int x, int y, float sectorSize, uint32_t id, Sector* neighbors[8])
{
    coordX = x;
    coordY = y;
    this->sectorSize = sectorSize;
    worldPosX = coordX * sectorSize + sectorSize / 2;
    worldPosY = coordY * sectorSize + sectorSize / 2;
    this->id = id;
    for(int i = 0; i < 8; i++)
    {
        this->neighbors[i] = neighbors[i];
    }
}

}  // namespace world