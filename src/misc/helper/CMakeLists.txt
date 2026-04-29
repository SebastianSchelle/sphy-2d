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
    command-node.cpp
)

set(LIBS_PUB
    yaml-cpp
    spdlog
    bitsery
    concurrentqueue
)

add_library(helper INTERFACE)
target_sources(helper INTERFACE ${HELPER_SOURCES})
target_include_directories(helper INTERFACE ${HELPER_INC_PUBLIC})
target_link_libraries(helper INTERFACE ${LIBS_PUB})
