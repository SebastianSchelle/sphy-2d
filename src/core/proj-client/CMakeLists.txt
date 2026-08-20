add_library(sphy_core_proj_client INTERFACE)
target_sources(sphy_core_proj_client INTERFACE
    proj-client.cpp
)

target_include_directories(sphy_core_proj_client
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_core_proj_client
    INTERFACE
)
