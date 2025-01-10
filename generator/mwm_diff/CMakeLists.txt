project(mwm_diff)

set(SRC
  diff.cpp
  diff.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
  coding
)

omim_add_tool_subdirectory(mwm_diff_tool)

omim_add_test_subdirectory(mwm_diff_tests)
