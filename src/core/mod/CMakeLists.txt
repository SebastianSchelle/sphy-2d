add_library(sphy_core_mod_common INTERFACE)
target_sources(sphy_core_mod_common INTERFACE
    mod-manager.cpp
    sphy-bindings.cpp
)

add_library(sphy_core_mod_client INTERFACE)
target_sources(sphy_core_mod_client INTERFACE
    mod-manager-client.cpp
)

target_include_directories(sphy_core_mod_common
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(sphy_core_mod_client
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

