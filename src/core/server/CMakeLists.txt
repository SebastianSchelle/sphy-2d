set(EXEC_NAME game-server)

set(MISC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../misc)
set(THIRPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)

set(SERVER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRPARTY_DIR}/entt/src
    ${THIRPARTY_DIR}/magic_enum/include
    ${THIRPARTY_DIR}/dascript/include
)

set(SERVER_SOURCES
    engine.cpp
    server.cpp
    main.cpp
)

set(LIBS_PUB
    helper
    networking
    concurrentqueue
    libDaScript
    sphy_core_world
    sphy_core_ecs
    sphy_core_game_objects_libs
    sphy_core_mod_common
    sphy_misc_runtime
)

set(LIBS_PRIV
    Boost::thread
    Boost::program_options
)

add_executable(${EXEC_NAME} ${SERVER_SOURCES})
if(MSVC)
    set(_das_mod_no_rtti /GR-)
else()
    set(_das_mod_no_rtti -fno-rtti)
endif()

set_property(
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../mod/mod-manager.cpp
    TARGET ${EXEC_NAME}
    PROPERTY COMPILE_OPTIONS "${_das_mod_no_rtti}")
set_property(
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../mod/sphy-bindings.cpp
    TARGET ${EXEC_NAME}
    PROPERTY COMPILE_OPTIONS "${_das_mod_no_rtti}")
target_compile_definitions(${EXEC_NAME} PRIVATE SERVER GAME_NAME="${GAME_NAME}")

target_include_directories(${EXEC_NAME} PUBLIC ${SERVER_INC_PUBLIC})
target_link_libraries(${EXEC_NAME} PUBLIC ${LIBS_PUB})
target_link_libraries(${EXEC_NAME} PRIVATE ${LIBS_PRIV})

# libDaScript ships uriparser; Arrow (rerun SDK) also embeds the same C objects
# in libarrow — duplicate global symbols. Prefer first definition.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
    (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
)
    target_link_options(
        ${EXEC_NAME}
        PRIVATE
        "LINKER:--allow-multiple-definition"
    )
endif()

