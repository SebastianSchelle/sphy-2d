set(LIB_NAME sphy-2d-server)

set(MISC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../misc)
set(THIRPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)

set(SERVER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mod
    ${MISC_DIR}/data-definitions
    ${MISC_DIR}/containers
    ${THIRPARTY_DIR}/sol2/include
    ../world
)

set(SERVER_INC_PRIVATE
)

set(SERVER_SOURCES
    engine.cpp
    server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../world/world.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../world/sector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../mod/mod-manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../mod/lua-interpreter.cpp
)

set(LIBS_PUB
    helper
    networking
    concurrentqueue
    ${LUA_LIBRARIES}
)

set(LIBS_PRIV
    Boost::thread
    Boost::program_options
)

add_library(${LIB_NAME} STATIC ${SERVER_SOURCES})
target_compile_definitions(${LIB_NAME} PRIVATE SERVER)

target_include_directories(${LIB_NAME} PUBLIC ${SERVER_INC_PUBLIC})
target_include_directories(${LIB_NAME} PRIVATE ${SERVER_INC_PRIVATE})
target_link_libraries(${LIB_NAME} PUBLIC ${LIBS_PUB})
target_link_libraries(${LIB_NAME} PRIVATE ${LIBS_PRIV})

