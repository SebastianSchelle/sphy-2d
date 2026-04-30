add_library(sphy_core_ecs_systems INTERFACE)

target_sources(sphy_core_ecs_systems INTERFACE
    ptr-handle.hpp
    sys-ai.hpp
    sys-phy.hpp
)

target_include_directories(sphy_core_ecs_systems
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
