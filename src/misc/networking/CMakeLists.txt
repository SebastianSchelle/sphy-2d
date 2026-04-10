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

add_library(networking OBJECT ${NETWORKING_SOURCES})

if(APPLE)
    target_include_directories(networking PRIVATE
        /opt/homebrew/opt/boost/include
    )

    target_link_directories(networking PRIVATE
        /opt/homebrew/opt/boost/lib
    )

    target_link_libraries(networking PUBLIC
        helper
    )
else()
    target_link_libraries(networking PUBLIC
        Boost::system
        helper
    )
endif()

target_include_directories(networking PUBLIC ${NETWORKING_INC_PUBLIC})
