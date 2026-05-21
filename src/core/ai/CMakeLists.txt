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

add_library(sphy_misc_ai STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/task-system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/task-basic.cpp
)

target_include_directories(sphy_misc_ai PUBLIC ${SPHY_AI_INC})

# PRIVATE: game-client also links this lib via sphy_misc_runtime; must not leak SERVER.
sphy_link_target_kind(sphy_misc_ai server PRIVATE)

# Header-only / compile-time deps only — do not link INTERFACE targets that own
# .cpp sources (ecs, world), or CMake merges their sources into this library.
target_link_libraries(sphy_misc_ai
    PUBLIC
    helper
    sphy_misc_data_definitions
    sphy_misc_containers
    EnTT::EnTT
    magic_enum::magic_enum
)
