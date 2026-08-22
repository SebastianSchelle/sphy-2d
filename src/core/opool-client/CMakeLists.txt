add_library(sphy_core_opool_client INTERFACE)
target_sources(sphy_core_opool_client INTERFACE
)

target_include_directories(sphy_core_opool_client
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_core_opool_client
    INTERFACE
)
