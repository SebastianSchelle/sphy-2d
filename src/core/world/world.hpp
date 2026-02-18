#ifndef WORLD_HPP
#define WORLD_HPP

#include <std-inc.hpp>
#include <world-def.hpp>
#include <matrix2d.hpp>
#include <sector.hpp>

namespace world
{

class World
{
  public:
    World();
    ~World();
    void init(def::WorldShape worldShape);
    Sector* getNeighboringSector(uint32_t x, uint32_t y, def::Direction dir);

  private:
    def::WorldShape worldShape;
    con::Matrix2D<Sector> sectors;
};

}

#endif