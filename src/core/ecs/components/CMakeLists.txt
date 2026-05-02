add_library(sphy_core_ecs_components INTERFACE)

target_sources(sphy_core_ecs_components INTERFACE
    comp-ai.hpp
    comp-gfx.hpp
    comp-ident.hpp
    comp-phy.cpp
    comp-phy.hpp
    comp-storage.hpp
    comp-struct.hpp
    comp-tag.hpp
)

target_include_directories(sphy_core_ecs_components
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
