add_subdirectory(systems)

add_library(sphy_core_ecs INTERFACE)
target_sources(sphy_core_ecs INTERFACE
    sector-registry.cpp
    registry-mapping.cpp
    systems.cpp
)

target_link_libraries(sphy_core_ecs
    INTERFACE
    sphy_core_ecs_systems
    sphy_core_ecs_components
)

target_include_directories(sphy_core_ecs
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
