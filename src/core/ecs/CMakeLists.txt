add_library(sphy_core_ecs INTERFACE)
target_sources(sphy_core_ecs INTERFACE
    ecs.cpp
    asset-factory.cpp
)

target_include_directories(sphy_core_ecs
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/systems
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)
