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

find_package(Boost REQUIRED COMPONENTS regex thread system chrono program_options)
find_package(GTest REQUIRED)

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
add_subdirectory(src/common/helper helper)
add_subdirectory(src/common/networking networking)
add_subdirectory(src/core/server)
add_subdirectory(src/core/client)
add_subdirectory(test-app/test-server)
add_subdirectory(test-app/test-client)


enable_testing()

set(TEST_LIBS
    GTest::gtest_main
    helper
    Boost::system
    Boost::chrono
    Boost::thread
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
    ${CMAKE_SOURCE_DIR}/src/common/helper
    ${CMAKE_SOURCE_DIR}/thirdparty/bitsery/include
    ${CMAKE_SOURCE_DIR}/thirdparty/glm
)

include(GoogleTest)
gtest_discover_tests(test-shelf-allocator)
