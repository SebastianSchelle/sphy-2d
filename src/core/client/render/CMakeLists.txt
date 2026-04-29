add_library(sphy_client_render INTERFACE)

target_sources(sphy_client_render INTERFACE
    render-engine.cpp
    texture.cpp
    shader.cpp
)

target_include_directories(sphy_client_render
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
