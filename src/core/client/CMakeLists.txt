set(SERVER_SOURCES
    client-main.cpp
)

add_library(sphy-2d-client STATIC ${SERVER_SOURCES})
target_link_libraries(sphy-2d-client PUBLIC helper)

