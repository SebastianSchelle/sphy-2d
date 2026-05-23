set(SPHY_AI_INC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/basic
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs/components
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs/systems
    ${CMAKE_CURRENT_SOURCE_DIR}/../world
    ${CMAKE_CURRENT_SOURCE_DIR}/../game-objects/libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/../../misc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../misc/helper
    ${CMAKE_CURRENT_SOURCE_DIR}/../../misc/data-definitions
    ${CMAKE_CURRENT_SOURCE_DIR}/../../misc/containers
)

# INTERFACE: task sources compile in the executable (game-server) with its SERVER/CLIENT
# define. A STATIC library with sphy_link_target_kind(server) was linked into game-client
# via sphy_misc_runtime and broke the Mac client build.
add_library(sphy_misc_ai INTERFACE)

target_sources(sphy_misc_ai INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/task-system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/task-basic.cpp
)

target_include_directories(sphy_misc_ai INTERFACE ${SPHY_AI_INC})

target_link_libraries(sphy_misc_ai
    INTERFACE
    helper
    sphy_misc_data_definitions
    sphy_misc_containers
    EnTT::EnTT
    magic_enum::magic_enum
)
