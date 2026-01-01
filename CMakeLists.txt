cmake_minimum_required(VERSION 3.15...4.1)

project(
  SHPY-2d
  VERSION 1.0
  LANGUAGES CXX)

add_subdirectory(${PROJECT_SOURCE_DIR}/src/common/helper helper)
add_subdirectory(src/core/server)
add_subdirectory(src/core/client)

add_executable(test-server src/core/server/server-main.cpp)
target_link_libraries(test-server PRIVATE sphy-2d-server)

add_executable(test-client src/core/client/client-main.cpp)
target_link_libraries(test-client PRIVATE sphy-2d-client)