set(MISC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../misc)
set(THIRPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)

set(SERVER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MISC_DIR}/data-definitions
)

set(SERVER_INC_PRIVATE
)

set(SERVER_SOURCES
    engine.cpp
    server.cpp
)

set(LIBS_PUB
    helper
    networking
    concurrentqueue
)

set(LIBS_PRIV
    Boost::thread
    Boost::program_options
)

add_library(sphy-2d-server STATIC ${SERVER_SOURCES})

target_include_directories(sphy-2d-server PUBLIC ${SERVER_INC_PUBLIC})
target_include_directories(sphy-2d-server PRIVATE ${SERVER_INC_PRIVATE})
target_link_libraries(sphy-2d-server PUBLIC ${LIBS_PUB})
target_link_libraries(sphy-2d-server PRIVATE ${LIBS_PRIV})

