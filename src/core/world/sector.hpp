#ifndef SECTOR_HPP
#define SECTOR_HPP

#include <std-inc.hpp>

namespace world
{

class Sector
{
  public:
    Sector();
    ~Sector();
    void init(int x, int y, float sectorSize, uint32_t id, Sector* neighbors[8]);

  private:
    int coordX;            // Sector coord X
    int coordY;            // Sector coord Y
    float sectorSize;      // Sector size
    uint32_t id;           // Sector Id
    float worldPosX;       // Sector center X in world coords
    float worldPosY;       // Sector center Y in world coords
    Sector* neighbors[8];  // Neighboring Sectors (8 neighbors)
};

}  // namespace world

#endif