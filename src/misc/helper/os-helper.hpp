#ifndef OS_HELPER_HPP
#define OS_HELPER_HPP

#include <filesystem>
#include <string>

namespace osh
{

std::filesystem::path getExecutablePath();
std::filesystem::path getExecutableDir();

}

#endif