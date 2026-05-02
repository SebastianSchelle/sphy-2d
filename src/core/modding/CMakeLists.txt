set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)

add_library(sphy_core_modding INTERFACE)

target_sources(sphy_core_modding INTERFACE
    modding-tools.cpp
)

target_include_directories(sphy_core_modding INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../client
    ${THIRDPARTY_DIR}/magic_enum/include
)

target_link_libraries(sphy_core_modding INTERFACE
    helper
    RmlUi::RmlUi
)
