set(LIB_NAME sphy-2d-client)

set(CLIENT_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CLIENT_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CLIENT_INC_PRIVATE
    thirdparty/concurrentqueue/c_api
)

set(CLIENT_SOURCES
    client.cpp
    model.cpp
)

set(LIBS_PUB
    helper
    networking
    concurrentqueue
)

set(LIBS_PRIV
    Boost::thread
)

add_library(${LIB_NAME} STATIC ${CLIENT_SOURCES})

target_include_directories(${LIB_NAME} PUBLIC ${CLIENT_INC_PUBLIC})
target_include_directories(${LIB_NAME} PRIVATE ${CLIENT_INC_PRIVATE})
target_link_libraries(${LIB_NAME} PUBLIC ${LIBS_PUB})
target_link_libraries(${LIB_NAME} PRIVATE ${LIBS_PRIV})

