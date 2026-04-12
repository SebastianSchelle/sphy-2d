# Shared build configurations (included from sphy-2d and from limes-vitae root).
include_guard(GLOBAL)

# =============================================================================
# Build types: Debug | Release | Profile
# =============================================================================
#   Debug   — CMake’s Debug flags; parent projects add DEBUG=1 for Debug only.
#   Release — CMake’s Release flags (optimized, NDEBUG).
#   Profile — Like Release + debug symbols + frame pointers (Linux perf, etc.).
#
# Single-config (Make, Unix Makefiles, default on many platforms):
#   cmake -S <source> -B <build> -DCMAKE_BUILD_TYPE=Release
#   cmake --build <build>
#
# Multi-config (Visual Studio, Xcode):
#   cmake --build <build> --config Profile
#
# Standalone sphy-2d example:
#   cmake -S sphy-2d -B sphy-2d/build -DCMAKE_BUILD_TYPE=Debug
#   cmake --build sphy-2d/build
# =============================================================================

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES
        "Debug;Release;Profile"
        CACHE STRING "Available configurations (multi-config generators)" FORCE)
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE
            Release
            CACHE STRING "Build type: Debug | Release | Profile" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release Profile)
endif()

if(MSVC)
    set(_sphy_profile_cxx "/O2 /Zi /DNDEBUG")
else()
    set(_sphy_profile_cxx "-O2 -g -fno-omit-frame-pointer -DNDEBUG")
endif()

# First configure seeds the cache; later runs keep a user override.
set(CMAKE_CXX_FLAGS_PROFILE
    "${_sphy_profile_cxx}"
    CACHE STRING
        "C++ flags for Profile (GCC/Clang: perf-friendly stacks; MSVC: optimized + PDB)")

mark_as_advanced(CMAKE_CXX_FLAGS_PROFILE)
