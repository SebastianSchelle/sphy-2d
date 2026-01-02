set(CLIENT_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CLIENT_SOURCES
    sphy-client.cpp
)

add_library(sphy-2d-client STATIC ${CLIENT_SOURCES})
target_include_directories(sphy-2d-client PUBLIC ${CLIENT_INC_PUBLIC})
target_link_libraries(sphy-2d-client PUBLIC helper)

