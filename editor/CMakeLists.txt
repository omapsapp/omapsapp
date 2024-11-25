project(editor)

set(SRC
  changeset_wrapper.cpp
  changeset_wrapper.hpp
  config_loader.cpp
  config_loader.hpp
  editable_data_source.hpp
  editable_feature_source.cpp
  editable_feature_source.hpp
  editor_config.cpp
  editor_config.hpp
  editor_notes.cpp
  editor_notes.hpp
  editor_storage.cpp
  editor_storage.hpp
  edits_migration.cpp
  edits_migration.hpp
  feature_matcher.cpp
  feature_matcher.hpp
  new_feature_categories.cpp
  new_feature_categories.hpp
  opening_hours_ui.cpp
  opening_hours_ui.hpp
  osm_auth.cpp
  osm_auth.hpp
  osm_editor.cpp
  osm_editor.hpp
  profile_picture.cpp
  profile_picture.hpp
  server_api.cpp
  server_api.hpp
  ui2oh.cpp
  ui2oh.hpp
  xml_feature.cpp
  xml_feature.hpp
  yes_no_unknown.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
  indexer
  opening_hours
  pugixml
)

omim_add_test_subdirectory(editor_tests)
omim_add_test_subdirectory(editor_tests_support)
omim_add_test_subdirectory(osm_auth_tests)
