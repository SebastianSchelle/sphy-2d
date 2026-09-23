
add_library(sphy_core_faction INTERFACE)

target_sources(sphy_core_faction INTERFACE
    faction.cpp
)

target_include_directories(sphy_core_faction INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_core_faction INTERFACE
    helper
)
