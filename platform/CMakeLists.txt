project(platform)

include_directories(${OMIM_ROOT}/3party/jansson/src ${OMIM_ROOT}/platform)

if(NOT PLATFORM_ANDROID AND NOT PLATFORM_IPHONE)
  set(CMAKE_AUTOMOC ON)
endif()

set(
  SRC
  ../private.h  # To properly detect its changes with CMake.
  battery_tracker.cpp
  battery_tracker.hpp
  chunks_download_strategy.cpp
  chunks_download_strategy.hpp
  constants.hpp
  country_defines.cpp
  country_defines.hpp
  country_file.cpp
  country_file.hpp
  downloader_defines.hpp
  downloader_utils.cpp
  downloader_utils.hpp
  file_logging.cpp
  file_logging.hpp
  get_text_by_id.cpp
  get_text_by_id.hpp
  gui_thread.hpp
  http_client.cpp
  http_client.hpp
  http_payload.cpp
  http_payload.hpp
  http_request.cpp
  http_request.hpp
  http_thread_callback.hpp
  http_uploader.hpp
  http_uploader_background.hpp
  local_country_file.cpp
  local_country_file.hpp
  local_country_file_utils.cpp
  local_country_file_utils.hpp
  locale.hpp
  localization.cpp
  localization.hpp
  location.hpp
  measurement_utils.cpp
  measurement_utils.hpp
  mwm_traits.cpp
  mwm_traits.hpp
  mwm_version.cpp
  mwm_version.hpp
  platform.cpp
  platform.hpp
  preferred_languages.cpp
  preferred_languages.hpp
  remote_file.cpp
  remote_file.hpp
  secure_storage.hpp
  servers_list.cpp
  servers_list.hpp
  settings.cpp
  settings.hpp
  socket.hpp
  string_storage_base.cpp
  string_storage_base.hpp
)

if(${PLATFORM_IPHONE})
  append(
    SRC
    background_downloader_ios.h
    background_downloader_ios.mm
    gui_thread_apple.mm
    http_thread_apple.h
    http_thread_apple.mm
    http_client_apple.mm
    http_uploader_apple.mm
    http_user_agent_ios.mm
    localization.mm
    locale.mm
    network_policy_ios.h
    network_policy_ios.mm
    platform_ios.mm
    platform_unix_impl.cpp
    platform_unix_impl.hpp
    secure_storage_ios.mm
    socket_apple.mm
  )
elseif(${PLATFORM_ANDROID})
  append(
    SRC
    platform_android.cpp
    platform_unix_impl.cpp
    platform_unix_impl.hpp
  )
else() # neither iPhone nor Android
  #
  # There is no reliable cross-platform way to generate version from git.
  # Use the current date for desktop.
  #
  string(TIMESTAMP VERSION "%Y.%m.%d" UTC)
  string(TIMESTAMP INT_VERSION "%y%m%d" UTC)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/platform_qt_version.cpp.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/platform_qt_version.cpp"
                  @ONLY)

  append(
    SRC
    localization_dummy.cpp
    location_service.cpp
    location_service.hpp
    network_policy_dummy.cpp
    platform_qt.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/platform_qt_version.cpp"
    wifi_info.hpp
    wifi_location_service.cpp
  )

  if (${PLATFORM_WIN})
    append(
      SRC
      http_client_curl.cpp
      http_thread_qt.cpp
      http_thread_qt.hpp
      http_uploader_background_dummy.cpp
      http_uploader_dummy.cpp
      platform_win.cpp
      secure_storage_dummy.cpp
      wifi_info_windows.cpp
    )
  elseif(${PLATFORM_MAC})
    append(
      SRC
      apple_location_service.mm
      gui_thread_apple.mm
      http_client_apple.mm
      http_thread_apple.h
      http_thread_apple.mm
      http_uploader_apple.mm
      http_uploader_background_dummy.cpp
      locale.mm
      platform_mac.mm
      platform_unix_impl.cpp
      platform_unix_impl.hpp
      secure_storage_qt.cpp
      socket_apple.mm
      http_session_manager.mm
    )
  elseif(${PLATFORM_LINUX})
    append(
      SRC
      gui_thread_linux.cpp
      http_client_curl.cpp
      http_thread_qt.cpp
      http_thread_qt.hpp
      http_uploader_dummy.cpp
      http_uploader_background_dummy.cpp
      locale_linux.cpp
      platform_linux.cpp
      platform_unix_impl.cpp
      platform_unix_impl.hpp
      secure_storage_qt.cpp
    )
  endif()
endif()

add_subdirectory(platform_tests_support)

omim_add_library(${PROJECT_NAME} ${SRC})

if (APPLE)
  target_compile_options(${PROJECT_NAME} PUBLIC "-fobjc-arc")
endif()

if (PLATFORM_LINUX)
  link_qt5_network(${PROJECT_NAME})
endif()

omim_add_test_subdirectory(platform_tests)
