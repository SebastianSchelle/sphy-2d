set(NETWORKING_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(NETWORKING_SOURCES
    tcp-server.cpp
    udp-server.cpp
    udp-client.cpp
    tcp-client.cpp
)

set(LIBS_PUB
    Boost::system
    helper
)

add_library(networking OBJECT ${NETWORKING_SOURCES})

target_include_directories(networking PUBLIC ${NETWORKING_INC_PUBLIC})

target_link_libraries(networking PUBLIC ${LIBS_PUB})
