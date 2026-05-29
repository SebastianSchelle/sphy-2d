#ifndef COMP_LIFETIME_HPP
#define COMP_LIFETIME_HPP

#include <std-inc.hpp>

namespace ecs
{

struct Lifetime
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "lifetime";
    float lifetime = 0.0f;
};

#define SER_LIFETIME                                                           \
    S4b(o.lifetime);
EXT_SER(Lifetime, SER_LIFETIME)
EXT_DES(Lifetime, SER_LIFETIME)


}  // namespace ecs

EXT_FMT(ecs::Lifetime, "lifetime: {}", o.lifetime);

#endif // COMP_LIFETIME_HPP