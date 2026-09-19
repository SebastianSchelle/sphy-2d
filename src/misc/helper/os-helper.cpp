#include <limits.h>
#include <os-helper.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux__) || defined(__linux)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdlib.h>
#endif

namespace osh
{
std::filesystem::path getExecutablePath()
{
#if defined(_WIN32) || defined(_WIN64)
    char exePath[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        // Error or path too long, fallback to current path
        return std::filesystem::current_path();
    }
    return std::filesystem::path(exePath);

#elif defined(__linux__) || defined(__linux)
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, PATH_MAX - 1);
    if (len == -1)
    {
        // Error reading symlink, fallback to current path
        return std::filesystem::current_path();
    }
    exePath[len] = '\0';  // readlink doesn't null-terminate
    return std::filesystem::path(exePath);

#elif defined(__APPLE__)
    char exePath[PATH_MAX];
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(exePath, &size) != 0)
    {
        // Buffer too small or error, fallback to current path
        return std::filesystem::current_path();
    }

    // _NSGetExecutablePath may return a relative path, resolve it
    char resolvedPath[PATH_MAX];
    if (realpath(exePath, resolvedPath) != nullptr)
    {
        return std::filesystem::path(resolvedPath);
    }
    else
    {
        // realpath failed, use the path as-is
        return std::filesystem::path(exePath);
    }

#else
    // Unknown platform, fallback to current path
    return std::filesystem::current_path();
#endif
}

std::filesystem::path getExecutableDir()
{
    return getExecutablePath().parent_path();
}

#if defined(__linux__)

#include <limits.h>
#include <unistd.h>

std::filesystem::path executablePath()
{
    char buffer[PATH_MAX];

    const ssize_t length =
        ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

    if (length <= 0)
        return {};

    buffer[length] = '\0';

    return std::filesystem::path(buffer);
}

#elif defined(__APPLE__)

#include <mach-o/dyld.h>

std::filesystem::path executablePath()
{
    uint32_t size = 0;

    if (_NSGetExecutablePath(nullptr, &size) != -1)
        return {};

    std::string buffer(size, '\0');

    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        return {};

    return std::filesystem::path(buffer.c_str());
}

#elif _WIN32

#define NOMINMAX
#include <windows.h>

std::filesystem::path executablePath()
{
    std::wstring buffer(256, L'\0');

    while (true)
    {
        const DWORD length = ::GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (length == 0)
            return {};

        if (length < buffer.size() - 1)
        {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }

        buffer.resize(buffer.size() * 2);
    }
}

#endif

}  // namespace osh