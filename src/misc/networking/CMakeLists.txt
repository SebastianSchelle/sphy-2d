set(NETWORKING_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(NETWORKING_SOURCES
    tcp-server.cpp
    udp-server.cpp
    udp-client.cpp
    tcp-client.cpp
    protocol.cpp
)

set(LIBS_PUB
    helper
)

add_library(networking INTERFACE)

target_sources(networking INTERFACE ${NETWORKING_SOURCES})
target_include_directories(networking INTERFACE ${NETWORKING_INC_PUBLIC})

target_link_libraries(networking INTERFACE ${LIBS_PUB})
