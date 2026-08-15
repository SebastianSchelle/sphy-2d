
add_library(sphy_misc_ai INTERFACE)

target_sources(sphy_misc_ai INTERFACE
    task-system.cpp
    basic/task-basic.cpp
    module/task-turret.cpp
)

target_include_directories(sphy_misc_ai INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    basic
    module
)

target_link_libraries(sphy_misc_ai
    INTERFACE
    helper
    sphy_misc_data_definitions
    sphy_misc_containers
    EnTT::EnTT
    magic_enum::magic_enum
    sphy_core_ecs
    sphy_core_ecs_components
)
