project(map_tests)

set(SRC
  address_tests.cpp
  bookmarks_test.cpp
  chart_generator_tests.cpp
  check_mwms.cpp
  countries_names_tests.cpp
  extrapolator_tests.cpp
  feature_getters_tests.cpp
  gps_track_collection_test.cpp
  gps_track_storage_test.cpp
  gps_track_test.cpp
  kmz_unarchive_test.cpp
  mwm_url_tests.cpp
  power_manager_tests.cpp
  search_api_tests.cpp
  transliteration_test.cpp
  working_time_tests.cpp
  elevation_info_tests.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC} REQUIRE_QT REQUIRE_SERVER)

target_link_libraries(${PROJECT_NAME}
  search_tests_support
  generator_tests_support
  map
)
