set(LIB_NAME sphy-2d-client)

set(THIRPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

set(BGFX_DIR ${CMAKE_SOURCE_DIR}/thirdparty/bgfx)
set(CLIENT_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common/data-definitions
    ${THIRPARTY_DIR}/glfw/include
    ${THIRPARTY_DIR}/rmlui/Include
    ${THIRPARTY_DIR}/rectpack2D/src
    ${BGFX_DIR}/bgfx/include
    ${BGFX_DIR}/bgfx/examples/common
    ${BGFX_DIR}/bgfx/3rdparty
    ${BGFX_DIR}/bx/include
    ${BGFX_DIR}/bimg/include
    ui
    win
    render
)

set(CLIENT_INC_PRIVATE
)

set(CLIENT_SOURCES
    client.cpp
    model.cpp
    win/main-window.cpp
    render/render-engine.cpp
    render/texture.cpp
    render/shader.cpp
    ui/rmlui-renderinterface.cpp
)

set(LIBS_PUB
    helper
    networking
    concurrentqueue
)

set(BGFX_BUILD ${THIRPARTY_DIR}/bgfx/cmake-build/cmake)
set(LIBS_PRIV
    "${BGFX_BUILD}/bgfx/libbgfx.a"
    "${BGFX_BUILD}/bgfx/libexample-common.a"
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
    glfw
    freetype
    RmlUi::RmlUi
    rectpack2D
)

file(GLOB SHADER_SRC CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/shader/**/fs_*.sc"
    "${CMAKE_SOURCE_DIR}/shader/**/vs_*.sc"
)

set(SHADERC "${BGFX_BUILD}/bgfx/shaderc")
set(SHADER_BIN_DIR ${CMAKE_SOURCE_DIR}/core-client/shader)
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
            -i ${CMAKE_SOURCE_DIR}/thirdparty/bgfx/bgfx/src
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${FILE_NAME}"
        VERBATIM
    )

    list(APPEND SHADER_BINARIES ${OUTPUT_FILE})
endforeach()

add_custom_target(shaders ALL
    DEPENDS ${SHADER_BINARIES}
)

add_library(${LIB_NAME} STATIC ${CLIENT_SOURCES})
add_dependencies(${LIB_NAME} shaders)
target_compile_definitions(${LIB_NAME} PRIVATE BX_CONFIG_DEBUG=1)

target_include_directories(${LIB_NAME} PUBLIC ${CLIENT_INC_PUBLIC})
target_include_directories(${LIB_NAME} PRIVATE ${CLIENT_INC_PRIVATE})
target_link_libraries(${LIB_NAME} PUBLIC ${LIBS_PUB})
target_link_libraries(${LIB_NAME} PRIVATE ${LIBS_PRIV})

