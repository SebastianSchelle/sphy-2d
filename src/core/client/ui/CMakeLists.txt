add_library(sphy_client_ui INTERFACE)

target_sources(sphy_client_ui INTERFACE
    rmlui-renderinterface.cpp
    rmlui-systeminterface.cpp
    user-interface.cpp
)

target_include_directories(sphy_client_ui
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_client_ui
    INTERFACE
    sphy_client_render
)
