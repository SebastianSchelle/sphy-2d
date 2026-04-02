#ifndef VERSION_HPP
#define VERSION_HPP

#include <std-inc.hpp>

namespace version
{

const uint16_t MAJOR = 1;
const uint16_t MINOR = 1;
const uint16_t PATCH = 0;

const std::string VERSION_STRING = fmt::format("{}.{}.{}", MAJOR, MINOR, PATCH);

}  // namespace version

#endif