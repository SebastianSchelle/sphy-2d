set(SERVER_SOURCES
    server-main.cpp
)

add_library(sphy-2d-server STATIC ${SERVER_SOURCES})
target_link_libraries(sphy-2d-server PUBLIC helper)

