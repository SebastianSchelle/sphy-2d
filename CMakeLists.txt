cmake_minimum_required(VERSION 3.14...4.1)
set (CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Backend selection
option(USE_WAYLAND "Enable Wayland backend for GLFW" OFF)

# GLFW backend configuration
if (USE_WAYLAND)
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
add_subdirectory(src/common/helper helper)
add_subdirectory(src/common/networking networking)
add_subdirectory(src/core/server)
add_subdirectory(src/core/client)
add_subdirectory(test/test-server)
add_subdirectory(test/test-client)
