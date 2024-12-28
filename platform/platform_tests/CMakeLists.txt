project(platform_tests)

set(SRC
  apk_test.cpp
  country_file_tests.cpp
  distance_tests.cpp
  duration_tests.cpp
  downloader_tests/downloader_test.cpp
  downloader_utils_tests.cpp
  get_text_by_id_tests.cpp
  jansson_test.cpp
  language_test.cpp
  local_country_file_tests.cpp
  location_test.cpp
  measurement_tests.cpp
  platform_test.cpp
  meta_config_tests.cpp
  products_tests.cpp
  utm_mgrs_utils_tests.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC} REQUIRE_QT REQUIRE_SERVER)

target_link_libraries(${PROJECT_NAME}
  platform_tests_support
  platform
  cppjansson
)
