cmake_minimum_required(VERSION 3.14...4.1)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Backend selection
option(USE_WAYLAND "Enable Wayland backend for GLFW" OFF)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_Declare(rerun_sdk URL
    https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip)
FetchContent_MakeAvailable(rerun_sdk)

# GLFW backend configuration
if(USE_WAYLAND)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
else()
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()

project(
    SHPY-2d
    VERSION 1.0
    LANGUAGES CXX)

# Debug | Release | Profile — usage: see cmake/SphyBuildTypes.cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SphyBuildTypes.cmake")

# Define DEBUG only for Debug configuration.
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

set(GAME_NAME "sphy2d" CACHE STRING "Game name used for save files and binaries")

find_package(Boost REQUIRED COMPONENTS regex thread system chrono program_options)
find_package(GTest REQUIRED)

set(LUA_INCLUDE_DIR /usr/include/lua5.4)
set(LUA_LIBRARY /usr/lib64/liblua-5.4.so)
find_package(Lua 5.4 REQUIRED)

# add third party libraries
add_subdirectory(thirdparty/yaml-cpp yaml-cpp)
add_subdirectory(thirdparty/spdlog spdlog)
add_subdirectory(thirdparty/concurrentqueue concurrentqueue)
add_subdirectory(thirdparty/bitsery bitsery)
add_subdirectory(thirdparty/glfw)
if(TARGET glfw)
    add_library(glfw::glfw ALIAS glfw)
endif()
add_subdirectory(thirdparty/freetype)
if(TARGET freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
add_subdirectory(thirdparty/rmlui)
add_subdirectory(thirdparty/rectpack2D)
add_subdirectory(thirdparty/sol2)
add_subdirectory(thirdparty/entt)
add_subdirectory(thirdparty/magic_enum)
# daScript as subproject: disable optional modules that expect standalone
# `${CMAKE_SOURCE_DIR}/modules/...` layout.
set(DAS_GLFW_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_AUDIO_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_STDDLG_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_STBIMAGE_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_TOOLS_DISABLED OFF CACHE BOOL "" FORCE)
set(DAS_TUTORIAL_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_TESTS_DISABLED ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/dascript)
add_subdirectory(src/misc/helper helper)
add_subdirectory(src/misc/networking networking)
add_subdirectory(src/core/server)
add_subdirectory(src/core/client)

set(DEPLOY_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/deploy.sh)
set(DEPLOY_DIR ${CMAKE_SOURCE_DIR}/deploy)

# Find bash
find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
    message(FATAL_ERROR "bash not found")
endif()
# Custom target to run deploy script
# Pass build directory and executable paths to the script
add_custom_target(deploy
    COMMAND ${BASH_EXECUTABLE} ${DEPLOY_SCRIPT} ${DEPLOY_DIR} ${CMAKE_BINARY_DIR} "$<TARGET_FILE:game-client>" "$<TARGET_FILE:game-server>"
    DEPENDS game-client game-server
    COMMENT "Deploying modules and executables to ${DEPLOY_DIR}"
)

add_custom_target(
    run-server
    COMMAND ${DEPLOY_DIR}/game-server -w ${DEPLOY_DIR} -s "$ENV{HOME}/Documents/galaxies/test-game-2"
    DEPENDS deploy
    COMMENT "Running server (save: ${RUN_SERVER_SAVE_DIR})"
)

add_custom_target(
    run-client
    COMMAND ${DEPLOY_DIR}/game-client -w ${DEPLOY_DIR}
    DEPENDS deploy
    COMMENT "Running client"
)

enable_testing()

set(TEST_LIBS
    GTest::gtest_main
    helper
    Boost::system
    Boost::chrono
    Boost::thread
    Boost::program_options
)

add_executable(
    test-shelf-allocator
    test/test-shelf-allocator.cpp
)
target_link_libraries(
    test-shelf-allocator
    PRIVATE
    ${TEST_LIBS}
)
target_include_directories(
    test-shelf-allocator
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/misc/helper
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/bitsery/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glm
)

include(GoogleTest)
gtest_discover_tests(test-shelf-allocator)

# Match Debug fusion level in Release (see cmake/SphyDaScriptFusionFix.cmake);
# do not modify thirdparty/daScript/CMake.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SphyDaScriptFusionFix.cmake")
sphy_das_fusion_zero_release(libDaScript game-server game-client)
