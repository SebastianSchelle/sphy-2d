add_library(sphy_core_ecs_components INTERFACE)

target_sources(sphy_core_ecs_components INTERFACE
    comp-phy.cpp
    comp-storage.cpp
    comp-struct.cpp
    comp-turret.cpp
    comp-collavoid.cpp
    client-registry.cpp
    asset-factory.cpp
)

target_include_directories(sphy_core_ecs_components
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_core_ecs_components
    INTERFACE
    sphy_core_obj_pool
)
