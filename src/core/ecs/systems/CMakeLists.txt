add_library(sphy_core_ecs_systems INTERFACE)

target_sources(sphy_core_ecs_systems INTERFACE
    sys-ai.cpp
    sys-phy.cpp
    sys-turret.cpp
    sys-lifetime.cpp
    sys-specsys.cpp
)

target_include_directories(sphy_core_ecs_systems
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_core_ecs_systems
    INTERFACE
    sphy_misc_ai
)