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

}  // namespace ecs

#endif