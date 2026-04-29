add_subdirectory(containers)
add_subdirectory(data-definitions)
add_subdirectory(helper)
add_subdirectory(networking)

add_library(sphy_misc_runtime INTERFACE)
target_sources(sphy_misc_runtime INTERFACE
    work-distributor.cpp
    work-sequencer.cpp
)

target_include_directories(sphy_misc_runtime
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_misc_runtime
    INTERFACE
    helper
    sphy_misc_ai
    sphy_misc_containers
    sphy_misc_data_definitions
)
