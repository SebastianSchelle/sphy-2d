set(EXEC_NAME_TC test-client)
set(DEPLOY_DIR_TC ${CMAKE_SOURCE_DIR}/deploy/${EXEC_NAME_TC})

set(INC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(SOURCES
    main.cpp
    test-client.cpp
)

add_executable(${EXEC_NAME_TC} ${SOURCES})
target_include_directories(${EXEC_NAME_TC} PUBLIC ${INC})
target_link_libraries(${EXEC_NAME_TC} PUBLIC sphy-2d-client)

message(STATUS "client deploy directory: ${DEPLOY_DIR_TC}")

add_custom_command(TARGET ${EXEC_NAME_TC} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${EXEC_NAME_TC}> ${DEPLOY_DIR_TC}
)

add_custom_command(TARGET ${EXEC_NAME_TC} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/game-dir ${DEPLOY_DIR_TC}
)