project(world_feed_integration_tests)

set(SRC world_feed_integration_tests.cpp)

omim_add_test(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
  world_feed
  generator
)

# This test requires additional data from the other repository
if(NOT TEST_DATA_REPO_URL)
	set(TEST_DATA_REPO_URL "https://github.com/organicmaps/world_feed_integration_tests_data.git")
endif()
set(DESTINATION_FOLDER "${OMIM_DATA_DIR}/test_data/world_feed_integration_tests_data/")

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND test -d ${DESTINATION_FOLDER} || (git clone ${TEST_DATA_REPO_URL} ${DESTINATION_FOLDER})
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
