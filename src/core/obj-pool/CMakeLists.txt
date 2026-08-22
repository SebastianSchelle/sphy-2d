add_library(sphy_core_obj_pool INTERFACE)
target_sources(sphy_core_obj_pool INTERFACE
)

target_include_directories(sphy_core_obj_pool
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_core_obj_pool
    INTERFACE
)
