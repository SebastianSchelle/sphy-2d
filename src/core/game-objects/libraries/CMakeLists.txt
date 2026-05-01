add_library(sphy_core_game_objects_libs INTERFACE)
target_sources(sphy_core_game_objects_libs INTERFACE
    lib-hull.cpp
    lib-textures.cpp
    lib-modules.cpp
    lib-collider.cpp
    lib-station-part.cpp
)

target_include_directories(sphy_core_game_objects_libs
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
