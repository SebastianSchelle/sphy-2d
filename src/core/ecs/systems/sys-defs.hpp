#ifndef SYS_DEFS_HPP
#define SYS_DEFS_HPP

#include <std-inc.hpp>

namespace world
{
    class Sector;
}

namespace ecs
{
struct PtrHandle;

typedef std::function<
    void(world::Sector*, float, PtrHandle*)>
    SystemFunction;

enum class SystemFlags
{
    ActiveSector = 0x0001,
    InactiveSector = 0x0002,
};
ENUM_BIN_OPS(SystemFlags)

struct System
{
    std::string name;
    SystemFlags sysFlags;
    SystemFunction function;

    bool operator==(const System& other) const
    {
        return name == other.name;
    }
    bool operator!=(const System& other) const
    {
        return !(*this == other);
    }
};
}  // namespace ecs

#endif