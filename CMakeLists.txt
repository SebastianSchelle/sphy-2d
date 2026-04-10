cmake_minimum_required(VERSION 3.14...4.1)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Backend selection
option(USE_WAYLAND "Enable Wayland backend for GLFW" OFF)

include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_Declare(rerun_sdk URL
    https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip)
FetchContent_GetProperties(rerun_sdk)
if(NOT rerun_sdk_POPULATED)
    FetchContent_Populate(rerun_sdk)
    set(RERUN_ARROW_LINK_SHARED ON CACHE BOOL "Use shared Arrow to avoid missing bundled static deps" FORCE)

    # CMake 4.x currently breaks Arrow's mimalloc sub-build in rerun's bundled
    # Arrow build flow; disable mimalloc in the fetched script.
    set(RERUN_ARROW_SCRIPT "${rerun_sdk_SOURCE_DIR}/download_and_build_arrow.cmake")
    file(READ "${RERUN_ARROW_SCRIPT}" RERUN_ARROW_SCRIPT_CONTENT)
    string(REPLACE "-DARROW_MIMALLOC=ON" "-DARROW_MIMALLOC=OFF"
        RERUN_ARROW_SCRIPT_CONTENT "${RERUN_ARROW_SCRIPT_CONTENT}")
    file(WRITE "${RERUN_ARROW_SCRIPT}" "${RERUN_ARROW_SCRIPT_CONTENT}")

    add_subdirectory("${rerun_sdk_SOURCE_DIR}" "${rerun_sdk_BINARY_DIR}")
endif()

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

if(APPLE)
    # macOS (Homebrew workaround)
    find_package(Boost REQUIRED)
else()
    # Linux / Windows (proper way)
    find_package(Boost REQUIRED COMPONENTS regex thread system chrono program_options)
endif()

# Define DEBUG only for Debug configuration.
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
find_package(GTest REQUIRED)

unset(LUA_INCLUDE_DIR CACHE)
unset(LUA_LIBRARY CACHE)
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
add_subdirectory(src/misc/helper helper)
add_subdirectory(src/misc/networking networking)
add_subdirectory(src/core/server)
add_subdirectory(src/core/client)

enable_testing()

set(TEST_LIBS
    GTest::gtest_main
    helper
)

add_executable(
    test-shelf-allocator
    test/test-shelf-allocator.cpp
)

if(APPLE)
    target_include_directories(test-shelf-allocator PRIVATE
        /opt/homebrew/opt/boost/include
    )

    target_link_directories(test-shelf-allocator PRIVATE
        /opt/homebrew/opt/boost/lib
    )
    target_link_libraries(test-shelf-allocator PRIVATE
        boost_thread
        boost_program_options
        boost_chrono
    )
else()
    target_link_libraries(test-shelf-allocator PRIVATE
        Boost::system
        Boost::chrono
        Boost::thread
        Boost::program_options
    )
endif()

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
