add_library(sphy_client_spine INTERFACE)

target_sources(sphy_client_spine INTERFACE
    spine-integration.cpp
)

target_include_directories(sphy_client_spine
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
