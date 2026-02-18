#ifndef WORLD_DEF_HPP
#define WORLD_DEF_HPP

#include <std-inc.hpp>

namespace def
{

enum class Direction
{
    N,
    NW,
    W,
    SW,
    S,
    SE,
    E,
    NE,
    NONE,
};


struct WorldShape
{
    uint32_t numSectorX;
    uint32_t numSectorY;
    float sectorSize;
};

EXT_SER(WorldShape, s.value4b(o.numSectorX); s.value4b(o.numSectorY);
        s.value4b(o.sectorSize, 100);)

}  // namespace def

#endif