add_library(sphy_core_world INTERFACE)
target_sources(sphy_core_world INTERFACE
    world.cpp
    sector.cpp
)

target_include_directories(sphy_core_world
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
