set(HELPER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(HELPER_SOURCES
    sphy-2d.cpp
)

add_library(helper OBJECT ${HELPER_SOURCES})
target_include_directories(helper PUBLIC ${HELPER_INC_PUBLIC})