cmake_minimum_required(VERSION 3.14...4.1)
set (CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
  SHPY-2d
  VERSION 1.0
  LANGUAGES CXX)

find_package(Boost REQUIRED COMPONENTS regex thread system chrono program_options)

# add third party libraries
add_subdirectory(thirdparty/yaml-cpp yaml-cpp)
add_subdirectory(thirdparty/spdlog spdlog)
add_subdirectory(thirdparty/concurrentqueue concurrentqueue)
add_subdirectory(src/common/helper helper)
add_subdirectory(src/core/server)
add_subdirectory(src/core/client)
add_subdirectory(test/test-server)
add_subdirectory(test/test-client)
