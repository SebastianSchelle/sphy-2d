set(EXEC_NAME_TS test-server)
set(DEPLOY_DIR_TS ${CMAKE_SOURCE_DIR}/deploy/${EXEC_NAME_TS})

set(INC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(SOURCES
    main.cpp
    test-server.cpp
)

add_executable(${EXEC_NAME_TS} ${SOURCES})
target_include_directories(${EXEC_NAME_TS} PUBLIC ${INC})
target_link_libraries(${EXEC_NAME_TS} PUBLIC sphy-2d-server)
add_dependencies(${EXEC_NAME_TS} cpm_ts)

message(STATUS "server deploy directory: ${DEPLOY_DIR_TS}")

add_custom_command(TARGET ${EXEC_NAME_TS} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${EXEC_NAME_TS}> ${DEPLOY_DIR_TS}/${EXEC_NAME_TS}
)

add_custom_target(cpm_ts ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/core-common ${DEPLOY_DIR_TS}/modules/core
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/core-server ${DEPLOY_DIR_TS}/modules/core
)
