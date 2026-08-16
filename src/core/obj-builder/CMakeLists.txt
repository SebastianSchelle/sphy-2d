add_library(sphy_core_game_objects_objb INTERFACE)
target_sources(sphy_core_game_objects_objb INTERFACE
    objb-recipes.cpp
)

target_include_directories(sphy_core_game_objects_objb
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../server
)

target_link_libraries(sphy_core_game_objects_objb
    INTERFACE
    sphy_misc_ai
)
