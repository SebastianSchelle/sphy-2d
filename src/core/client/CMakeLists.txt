set(EXEC_NAME game-client)

set(THIRPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../thirdparty)
set(BGFX_DIR ${THIRPARTY_DIR}/bgfx)
set(MISC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../misc)
add_subdirectory(render)
add_subdirectory(ui)

set(CLIENT_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRPARTY_DIR}/entt/src
    ${THIRPARTY_DIR}/magic_enum/include
    ${THIRPARTY_DIR}/dascript/include
    ${BGFX_DIR}/bgfx/include
    ${BGFX_DIR}/bgfx/examples/common
    ${BGFX_DIR}/bgfx/3rdparty
    ${BGFX_DIR}/bx/include
    ${BGFX_DIR}/bimg/include
    win
)

set(CLIENT_SOURCES
    client.cpp
    model.cpp
    main.cpp
    win/main-window.cpp
    ${MISC_DIR}/networking/exchange-sequence.cpp
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
    sphy_core_mod_client
    sphy_core_modding
    sphy_misc_runtime
    sphy_client_render
    sphy_client_ui
)

set(BGFX_BUILD ${BGFX_DIR}/build/cmake)
# bgfx core only — do not link libexample-common (entry framework + _main_ + X11
# window helpers); this client uses GLFW + bgfx::init() directly (see main-window.cpp).
set(LIBS_PRIV
    "${BGFX_BUILD}/bgfx/libbgfx.a"
    "${BGFX_BUILD}/bgfx/libfcpp.a"
    "${BGFX_BUILD}/bgfx/libglsl-optimizer.a"
    "${BGFX_BUILD}/bgfx/libglslang.a"
    "${BGFX_BUILD}/bgfx/libspirv-cross.a"
    "${BGFX_BUILD}/bgfx/libspirv-opt.a"
    "${BGFX_BUILD}/bimg/libbimg.a"
    "${BGFX_BUILD}/bimg/libbimg_encode.a"
    "${BGFX_BUILD}/bimg/libbimg_decode.a"
    "${BGFX_BUILD}/bx/libbx.a"
    Boost::thread
    Boost::program_options
    glfw
    freetype
    RmlUi::RmlUi
    rectpack2D
)

file(GLOB SHADER_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../shader/**/fs_*.sc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../shader/**/vs_*.sc"
)

set(SHADERC "${BGFX_BUILD}/bgfx/shaderc")
set(SHADER_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../data/modules/engine/shader)
set(SHADER_PROFILE spirv)
foreach(SHADER ${SHADER_SRC})   
    message(STATUS "Add shader compile target: ${SHADER}") 
    get_filename_component(FILE_NAME ${SHADER} NAME_WE)
    set(OUTPUT_FILE "${SHADER_BIN_DIR}/${FILE_NAME}.bin")
    string(SUBSTRING ${FILE_NAME} 0 1 TYPE)
    message(out file="${OUTPUT_FILE}")
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${SHADERC}
            -f ${SHADER}
            -o ${OUTPUT_FILE}
            --platform linux
            --type ${TYPE}
            --profile ${SHADER_PROFILE}
            -i  ${BGFX_DIR}/bgfx/src
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${FILE_NAME}"
        VERBATIM
    )

    list(APPEND SHADER_BINARIES ${OUTPUT_FILE})
endforeach()

add_custom_target(shaders ALL
    DEPENDS ${SHADER_BINARIES}
)

add_executable(${EXEC_NAME} ${CLIENT_SOURCES})
add_dependencies(${EXEC_NAME} shaders)
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
target_compile_definitions(${EXEC_NAME} PRIVATE BX_CONFIG_DEBUG=1 CLIENT GAME_NAME="${GAME_NAME}")

target_include_directories(${EXEC_NAME} PUBLIC ${CLIENT_INC_PUBLIC})
target_link_libraries(${EXEC_NAME} PUBLIC ${LIBS_PUB})
target_link_libraries(${EXEC_NAME} PRIVATE ${LIBS_PRIV})

