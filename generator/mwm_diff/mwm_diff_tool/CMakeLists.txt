project(mwm_diff_tool)

set(SRC mwm_diff_tool.cpp)

omim_add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} mwm_diff)
