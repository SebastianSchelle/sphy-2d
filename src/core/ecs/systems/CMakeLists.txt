add_library(sphy_core_ecs_systems INTERFACE)

target_sources(sphy_core_ecs_systems INTERFACE
    sys-ai.hpp
    sys-phy.hpp
    sys-phy.cpp
    sys-turret.hpp
    sys-turret.cpp
)

target_include_directories(sphy_core_ecs_systems
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
