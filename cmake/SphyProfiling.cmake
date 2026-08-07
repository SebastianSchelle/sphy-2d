# Linux perf profiling targets for game-server (Profile build type).
include_guard(GLOBAL)

option(SPHY_ENABLE_PROFILING_TARGETS
    "Add CMake targets to record and report game-server performance with perf"
    ON)

if(NOT SPHY_ENABLE_PROFILING_TARGETS)
    return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "SPHY profiling targets disabled (Linux + perf only)")
    return()
endif()

find_program(SPHY_PERF_EXECUTABLE perf)
if(NOT SPHY_PERF_EXECUTABLE)
    message(STATUS "SPHY profiling targets disabled (perf not found)")
    return()
endif()

set(SPHY_PROFILE_CALL_GRAPH "dwarf,8192" CACHE STRING
    "perf --call-graph mode (dwarf,8192 = source-friendly stacks; fp = faster, less detail)")
set(SPHY_PROFILE_FREQUENCY "997" CACHE STRING
    "perf sample frequency in Hz")

if(NOT BASH_EXECUTABLE)
    find_program(BASH_EXECUTABLE bash REQUIRED)
endif()

set(SPHY_PROFILE_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/profile-server.sh")
if(NOT EXISTS "${SPHY_PROFILE_SCRIPT}")
    message(FATAL_ERROR "Missing profiling script: ${SPHY_PROFILE_SCRIPT}")
endif()

set(SPHY_PROFILE_DATA "${CMAKE_BINARY_DIR}/perf.data")
set(SPHY_PROFILE_REPORT "${CMAKE_BINARY_DIR}/perf-report.txt")
set(SPHY_PROFILE_SOURCE_REPORT "${CMAKE_BINARY_DIR}/perf-report-source.txt")

if(NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "Profile")
    message(WARNING
        "SPHY profiling targets expect CMAKE_BUILD_TYPE=Profile (current: ${CMAKE_BUILD_TYPE}). "
        "Use: cmake --preset profile && cmake --build --preset profile")
endif()

set(_sphy_profile_env
    "SPHY_PROFILE_BINARY_DIR=${CMAKE_BINARY_DIR}"
    "SPHY_PROFILE_DATA=${SPHY_PROFILE_DATA}"
    "SPHY_PROFILE_DEPLOY_DIR=${DEPLOY_DIR}"
    "SPHY_PROFILE_SAVE_DIR=${RUN_SERVER_SAVE_DIR}/test"
    "SPHY_PROFILE_CALL_GRAPH=${SPHY_PROFILE_CALL_GRAPH}"
    "SPHY_PROFILE_FREQUENCY=${SPHY_PROFILE_FREQUENCY}"
)

function(_sphy_add_profile_target name mode)
    cmake_parse_arguments(ARG "USES_DEPLOY" "COMMENT" "EXTRA_ENV" ${ARGN})
    set(_deps "")
    if(ARG_USES_DEPLOY)
        list(APPEND _deps deploy)
    endif()
    set(_env ${_sphy_profile_env} ${ARG_EXTRA_ENV})
    add_custom_target(
        ${name}
        COMMAND ${CMAKE_COMMAND} -E env ${_env}
            ${BASH_EXECUTABLE} ${SPHY_PROFILE_SCRIPT} ${mode}
        DEPENDS ${_deps}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "${ARG_COMMENT}"
        USES_TERMINAL
        VERBATIM
    )
endfunction()

# Record until Ctrl+C, or set SPHY_PROFILE_DURATION=30 for a timed capture.
_sphy_add_profile_target(
    profile-server
    record
    USES_DEPLOY
    COMMENT "Recording server profile to ${SPHY_PROFILE_DATA} (Ctrl+C to stop)"
)

_sphy_add_profile_target(
    profile-server-report
    report
    COMMENT "Writing overhead report to ${SPHY_PROFILE_REPORT}"
    EXTRA_ENV "SPHY_PROFILE_OUTPUT=${SPHY_PROFILE_REPORT}"
)

_sphy_add_profile_target(
    profile-server-source
    report-source
    COMMENT "Writing source-annotated report to ${SPHY_PROFILE_SOURCE_REPORT}"
    EXTRA_ENV "SPHY_PROFILE_OUTPUT=${SPHY_PROFILE_SOURCE_REPORT}"
)

_sphy_add_profile_target(
    profile-server-top
    top
    COMMENT "Interactive top symbols from ${SPHY_PROFILE_DATA}"
)

message(STATUS "SPHY profiling targets: profile-server, profile-server-report, profile-server-source")
