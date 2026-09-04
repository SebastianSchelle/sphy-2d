#ifndef COMP_COLLAVOID
#define COMP_COLLAVOID

#include <std-inc.hpp>

namespace ecs
{

struct CollAvoid
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "coll-avoid";

    bool active = true;
    uint32_t nextRunFrame;
};

#define SER_COLL_AVOID                                                         \
    S1b(o.active);                                                             \
    S4b(o.nextRunFrame);
EXT_SER(CollAvoid, SER_COLL_AVOID)
EXT_DES(CollAvoid, SER_COLL_AVOID)

}  // namespace ecs

#endif