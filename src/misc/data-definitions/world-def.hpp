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

struct SectorPos
{
    uint32_t x;
    uint32_t y;
};

struct SectorCoords
{
    SectorPos pos;
    vec2 sectorPos;
};

struct WorldShape
{
    static const uint16_t VERSION = 1;
    uint32_t numSectorX;
    uint32_t numSectorY;
    float sectorSize;
};

#define SER_WORLD_SHAPE                                                        \
    S4b(o.numSectorX);                                                         \
    S4b(o.numSectorY);                                                         \
    S4b(o.sectorSize);

#define DES_WORLD_SHAPE                                                        \
    D4b(o.numSectorX);                                                         \
    D4b(o.numSectorY);                                                         \
    D4b(o.sectorSize);

EXT_SER(WorldShape, SER_WORLD_SHAPE)
EXT_DES(WorldShape, SER_WORLD_SHAPE)

}  // namespace def

#endif