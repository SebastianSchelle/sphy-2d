set(THIRPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)

set(HELPER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config-manager
    ${THIRPARTY_DIR}/bitsery/include
    ${THIRPARTY_DIR}/glm
)

set(HELPER_SOURCES
    sphy-2d.cpp
    os-helper.cpp
    config-manager/config-manager.cpp
    config-manager/config-node.cpp
)

set(LIBS_PUB
    yaml-cpp
    spdlog
    bitsery
)

add_library(helper OBJECT ${HELPER_SOURCES})
target_include_directories(helper PUBLIC ${HELPER_INC_PUBLIC})
target_link_libraries(helper PUBLIC ${LIBS_PUB})
