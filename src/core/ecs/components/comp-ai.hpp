#ifndef COMP_AI_HPP
#define COMP_AI_HPP

#include <std-inc.hpp>

namespace ecs
{

struct Ai
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "ai";

    GenericHandle stackHandle;
    uint32_t nextRunFrame;
};

#define SER_AI SOBJ(o.stackHandle);
EXT_SER(Ai, SER_AI)
EXT_DES(Ai, SER_AI)

}  // namespace ecs

EXT_FMT(ecs::Ai, "stackHandle: {}, nextRunFrame: {}", o.stackHandle, o.nextRunFrame);

#endif