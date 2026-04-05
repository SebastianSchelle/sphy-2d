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

    bool operator==(const SectorPos& other) const
    {
        return x == other.x && y == other.y;
    }
    bool operator!=(const SectorPos& other) const
    {
        return x != other.x || y != other.y;
    }
};

struct SectorCoords
{
    SectorPos pos;
    vec2 sectorPos;
    bool operator==(const SectorCoords& other) const
    {
        return pos == other.pos && sectorPos == other.sectorPos;
    }
    bool operator!=(const SectorCoords& other) const
    {
        return pos != other.pos || sectorPos != other.sectorPos;
    }
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

EXT_FMT(def::SectorPos, "({}, {})", o.x, o.y);
EXT_FMT(def::SectorCoords, "(s:{}, p:{})", o.pos, o.sectorPos);

#endif