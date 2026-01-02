set(HELPER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config-manager
)

set(HELPER_SOURCES
    sphy-2d.cpp
    config-manager/config-manager.cpp
    config-manager/config-node.cpp
)

add_library(helper OBJECT ${HELPER_SOURCES})

target_include_directories(helper PUBLIC ${HELPER_INC_PUBLIC})

target_link_libraries(helper PUBLIC yaml-cpp)
target_link_libraries(helper PUBLIC spdlog)
