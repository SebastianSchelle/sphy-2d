#include <world.hpp>

namespace world
{

World::World() {}

World::~World() {}

void World::init(def::WorldShape worldShape)
{
    sectors.init(worldShape.numSectorX, worldShape.numSectorY);
    for (int i = 0; i < worldShape.numSectorX; i++)
    {
        for (int j = 0; j < worldShape.numSectorY; j++)
        {
            Sector* neighbors[8];
            for (int k = 0; k < 8; k++)
            {
                neighbors[k] =
                    getNeighboringSector(i, j, static_cast<def::Direction>(k));
            }
            sectors.at(i, j)->init(i,
                                   j,
                                   worldShape.sectorSize,
                                   i * worldShape.numSectorY + j,
                                   neighbors);
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