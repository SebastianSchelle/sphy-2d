add_subdirectory(components)
add_subdirectory(systems)

add_library(sphy_core_ecs INTERFACE)
target_sources(sphy_core_ecs INTERFACE
    ecs.cpp
    asset-factory.cpp
)

target_link_libraries(sphy_core_ecs
    INTERFACE
    sphy_core_ecs_components
    sphy_core_ecs_systems
)

target_include_directories(sphy_core_ecs
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
