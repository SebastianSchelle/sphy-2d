set(SERVER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common/data-definitions
)

set(SERVER_INC_PRIVATE
    thirdparty/concurrentqueue/c_api
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
)

add_library(sphy-2d-server STATIC ${SERVER_SOURCES})

target_include_directories(sphy-2d-server PUBLIC ${SERVER_INC_PUBLIC})
target_include_directories(sphy-2d-server PRIVATE ${SERVER_INC_PRIVATE})
target_link_libraries(sphy-2d-server PUBLIC ${LIBS_PUB})
target_link_libraries(sphy-2d-server PRIVATE ${LIBS_PRIV})

