add_library(sphy_misc_ai INTERFACE)

set(THIRPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)

target_include_directories(sphy_misc_ai
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/basic
    ${THIRPARTY_DIR}/magic_enum/include
)

target_sources(sphy_misc_ai INTERFACE
    task-system.cpp
    basic/task-basic.cpp
)
