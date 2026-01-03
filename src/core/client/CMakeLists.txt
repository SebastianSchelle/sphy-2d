set(LIB_NAME sphy-2d-client)

set(THIRPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

set(BGFX_DIR ${CMAKE_SOURCE_DIR}/thirdparty/bgfx)
set(CLIENT_INC_PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common/data-definitions
    ${THIRPARTY_DIR}/glfw/include
    ${THIRPARTY_DIR}/rmlui/Include
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
)

add_library(${LIB_NAME} STATIC ${CLIENT_SOURCES})
target_compile_definitions(${LIB_NAME} PRIVATE BX_CONFIG_DEBUG=1)

target_include_directories(${LIB_NAME} PUBLIC ${CLIENT_INC_PUBLIC})
target_include_directories(${LIB_NAME} PRIVATE ${CLIENT_INC_PRIVATE})
target_link_libraries(${LIB_NAME} PUBLIC ${LIBS_PUB})
target_link_libraries(${LIB_NAME} PRIVATE ${LIBS_PRIV})

