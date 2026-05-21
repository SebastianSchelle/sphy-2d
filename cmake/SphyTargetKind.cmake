# Server vs client compile definitions for shared headers (e.g. PtrHandle).
#
# - Executables: link sphy_target_server or sphy_target_client (PUBLIC).
# - INTERFACE libraries (world, ecs, mod): do not attach a kind here unless the
#   library is used by a single executable only. Sources compile in the consumer
#   and inherit SERVER or CLIENT from that executable.
# - STATIC libraries: link the matching sphy_target_* (e.g. sphy_misc_ai → server).
#   Do not put sphy_target_server on INTERFACE libs also linked by game-client.
#
# Usage:
#   include(SphyTargetKind)  # from project root, before add_subdirectory(src)
#   sphy_link_target_kind(game-server server)
#   sphy_link_target_kind(sphy_misc_ai server)  # static lib built for server only

include_guard(GLOBAL)

add_library(sphy_target_server INTERFACE)
target_compile_definitions(sphy_target_server INTERFACE SERVER)

add_library(sphy_target_client INTERFACE)
target_compile_definitions(sphy_target_client INTERFACE CLIENT)

# visibility: PUBLIC for executables (propagate to INTERFACE .cpp sources they compile).
# PRIVATE for static libraries also linked by the other executable (e.g. sphy_misc_ai).
function(sphy_link_target_kind target kind)
    cmake_parse_arguments(ARG "PRIVATE" "" "" ${ARGN})
    set(_vis PUBLIC)
    if(ARG_PRIVATE)
        set(_vis PRIVATE)
    endif()
    if(kind STREQUAL "server")
        target_link_libraries(${target} ${_vis} sphy_target_server)
    elseif(kind STREQUAL "client")
        target_link_libraries(${target} ${_vis} sphy_target_client)
    else()
        message(FATAL_ERROR "sphy_link_target_kind: unknown kind '${kind}' (use server or client)")
    endif()
endfunction()
