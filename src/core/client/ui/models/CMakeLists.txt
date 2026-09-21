add_library(sphy_client_ui_models INTERFACE)

target_sources(sphy_client_ui_models INTERFACE
    dm-if.cpp
)

target_include_directories(sphy_client_ui_models
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_client_ui_models
    INTERFACE
)
