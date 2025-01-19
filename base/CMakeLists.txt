project(base)

set(SRC
  # Our code will be recompiled on modification of these headers.
  ../std/boost_container_hash.hpp
  ../std/boost_geometry.hpp
  ../std/glm_gtx_rotate_vector.hpp
  ../std/target_os.hpp
  ../std/windows.hpp
  array_adapters.hpp
  assert.hpp
  base.cpp
  base.hpp
  beam.hpp
  bidirectional_map.hpp
  bits.hpp
  buffer_vector.hpp
  cache.hpp
  cancellable.cpp
  cancellable.hpp
  checked_cast.hpp
  clustering_map.hpp
  collection_cast.hpp
  control_flow.hpp
  deferred_task.cpp
  deferred_task.hpp
  dfa_helpers.hpp
  exception.cpp
  exception.hpp
  fifo_cache.hpp
  file_name_utils.cpp
  file_name_utils.hpp
  geo_object_id.cpp
  geo_object_id.hpp
  gmtime.cpp
  gmtime.hpp
  internal/message.cpp
  internal/message.hpp
  levenshtein_dfa.cpp
  levenshtein_dfa.hpp
  limited_priority_queue.hpp
  linked_map.hpp
  logging.cpp
  logging.hpp
  lower_case.cpp
  lru_cache.hpp
  macros.hpp
  math.cpp
  math.hpp
  matrix.hpp
  mem_trie.hpp
  newtype.hpp
  normalize_unicode.cpp
  non_intersecting_intervals.hpp
  observer_list.hpp
  pprof.cpp
  pprof.hpp
  random.hpp
  range_iterator.hpp
  ref_counted.hpp
  rolling_hash.hpp
  scope_guard.hpp
  set_operations.hpp
  shared_buffer_manager.cpp
  shared_buffer_manager.hpp
  small_map.hpp
  small_set.hpp
  src_point.cpp
  src_point.hpp
  stats.hpp
  stl_helpers.hpp
  stl_iterator.hpp
  string_utils.cpp
  string_utils.hpp
  strings_bundle.cpp
  strings_bundle.hpp
  suffix_array.cpp
  suffix_array.hpp
  sunrise_sunset.cpp
  sunrise_sunset.hpp
  task_loop.hpp
  thread.cpp
  thread.hpp
  thread_checker.cpp
  thread_checker.hpp
  thread_pool.cpp
  thread_pool.hpp
  thread_pool_computational.hpp
  thread_pool_delayed.cpp
  thread_pool_delayed.hpp
  thread_safe_queue.hpp
  thread_utils.hpp
  threaded_container.cpp
  threaded_container.hpp
  threaded_list.hpp
  timegm.cpp
  timegm.hpp
  timer.cpp
  timer.hpp
  uni_string_dfa.cpp
  uni_string_dfa.hpp
  visitor.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} INTERFACE Threads::Threads)
target_link_libraries(${PROJECT_NAME} PUBLIC utf8cpp::utf8cpp)

if (NOT WITH_SYSTEM_PROVIDED_3PARTY)
  target_include_directories(${PROJECT_NAME} PRIVATE "${OMIM_ROOT}/3party/fast_double_parser/include")
endif()

omim_add_test_subdirectory(base_tests)
