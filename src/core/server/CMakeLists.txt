set(EXEC_NAME game-server)

set(MISC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../misc)
set(THIRPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)

set(SERVER_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mod
    ${MISC_DIR}/data-definitions
    ${MISC_DIR}/containers
    ${MISC_DIR}
    ${THIRPARTY_DIR}/entt/src
    ${THIRPARTY_DIR}/magic_enum/include
    ${THIRPARTY_DIR}/dascript/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../world
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs/systems
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs/components
    ${CMAKE_CURRENT_SOURCE_DIR}/../game-objects/libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set(SERVER_INC_PRIVATE
)

set(SERVER_SOURCES
    engine.cpp
    server.cpp
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../world/world.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../world/sector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../mod/mod-manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../mod/sphy-bindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs/ecs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs/asset-factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game-objects/libraries/lib-hull.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game-objects/libraries/lib-textures.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game-objects/libraries/lib-modules.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game-objects/libraries/lib-collider.cpp
    ${MISC_DIR}/work-distributor.cpp
    ${MISC_DIR}/work-sequencer.cpp
)

set(LIBS_PUB
    helper
    networking
    concurrentqueue
    libDaScript
)

set(LIBS_PRIV
    Boost::thread
    Boost::program_options
)

add_executable(${EXEC_NAME} ${SERVER_SOURCES})
# libDaScript is built with -fno-rtti; TUs that inherit das::* must match or
# link fails on missing base typeinfo in Release. Apply per-target so the same
# mod-manager.cpp can still use RTTI when compiled for sphy-2d-client (RmlUi).
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
target_include_directories(${EXEC_NAME} PRIVATE ${SERVER_INC_PRIVATE})
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

