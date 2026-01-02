set(EXEC_NAME test-server)
set(DEPLOY_DIR ${CMAKE_SOURCE_DIR}/deploy/${EXEC_NAME})

set(INC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(SOURCES
    main.cpp
    test-server.cpp
)

add_executable(${EXEC_NAME} ${SOURCES})
target_include_directories(${EXEC_NAME} PUBLIC ${INC})
target_link_libraries(${EXEC_NAME} PUBLIC sphy-2d-server)

add_custom_command(TARGET ${EXEC_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/game-dir ${DEPLOY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${EXEC_NAME}> ${DEPLOY_DIR}
)
