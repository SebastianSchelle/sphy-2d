# Shared build configurations (included from sphy-2d and from limes-vitae root).
include_guard(GLOBAL)

# =============================================================================
# Build types: Debug | Release | Profile
# =============================================================================
#   Debug   — CMake's Debug flags; parent projects add DEBUG=1 for Debug only.
#   Release — CMake's Release flags (optimized, NDEBUG).
#   Profile — Release-like optimizations with full debug info and frame pointers
#             for Linux perf (source-level reports, not assembly-first).
#
# Single-config (Make, Unix Makefiles, default on many platforms):
#   cmake -S <source> -B <build> -DCMAKE_BUILD_TYPE=Profile
#   cmake --build <build>
#
# Multi-config (Visual Studio, Xcode):
#   cmake --build <build> --config Profile
#
# Profiling workflow (Linux):
#   cmake --preset profile && cmake --build --preset profile
#   cmake --build build/profile --target profile-server        # record (Ctrl+C to stop)
#   cmake --build build/profile --target profile-server-report # text report
#   cmake --build build/profile --target profile-server-source # source-annotated report
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
    set(_sphy_profile_compile "/O2 /Zi /DNDEBUG")
    set(_sphy_profile_link "/DEBUG")
else()
    # -g3 + DWARF line tables keep perf/hotspot on source longer before assembly.
    # -fno-omit-frame-pointer / -mno-omit-leaf-frame-pointer: fast fp-based stacks.
    # -fno-optimize-sibling-calls: clearer parent/child attribution in reports.
    set(_sphy_profile_compile
        -O2
        -g3
        -gdwarf-4
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
        -DNDEBUG
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i686|x86)$")
        list(APPEND _sphy_profile_compile -mno-omit-leaf-frame-pointer)
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Clang-only: extended line tables for sampling profilers.
        list(APPEND _sphy_profile_compile -fdebug-info-for-profiling)
    endif()
    set(_sphy_profile_link "-Wl,--build-id=sha1")
endif()

string(JOIN " " _sphy_profile_compile_str ${_sphy_profile_compile})

# Seed Profile flags on first configure. An empty cached value (e.g. from an older
# CMake run) is treated as unset so Profile builds always get -g3.
function(_sphy_init_profile_cache_var _var _value _doc)
    if("${${_var}}" STREQUAL "")
        set(${_var} "${_value}" CACHE STRING "${_doc}" FORCE)
    else()
        set(${_var} "${${_var}}" CACHE STRING "${_doc}")
    endif()
endfunction()

_sphy_init_profile_cache_var(
    CMAKE_C_FLAGS_PROFILE
    "${_sphy_profile_compile_str}"
    "C flags for Profile (perf-friendly debug info and stacks)")
_sphy_init_profile_cache_var(
    CMAKE_CXX_FLAGS_PROFILE
    "${_sphy_profile_compile_str}"
    "C++ flags for Profile (perf-friendly debug info and stacks)")
_sphy_init_profile_cache_var(
    CMAKE_EXE_LINKER_FLAGS_PROFILE
    "${_sphy_profile_link}"
    "Linker flags for Profile (build-id for perf symbol lookup)")
_sphy_init_profile_cache_var(
    CMAKE_SHARED_LINKER_FLAGS_PROFILE
    "${_sphy_profile_link}"
    "Shared linker flags for Profile")

mark_as_advanced(
    CMAKE_C_FLAGS_PROFILE
    CMAKE_CXX_FLAGS_PROFILE
    CMAKE_EXE_LINKER_FLAGS_PROFILE
    CMAKE_SHARED_LINKER_FLAGS_PROFILE
)
