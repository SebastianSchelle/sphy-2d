add_library(sphy_core_game_objects_objb INTERFACE)
target_sources(sphy_core_game_objects_objb INTERFACE
    objb-ship.hpp
)

target_include_directories(sphy_core_game_objects_objb
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
