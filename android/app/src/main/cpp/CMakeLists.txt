cmake_minimum_required(VERSION 3.22.1)

project(organicmaps C CXX)

set(SRC
  # JNI headers
  ../../../../../private.h
  app/organicmaps/core/jni_helper.hpp
  app/organicmaps/core/jni_java_methods.hpp
  app/organicmaps/core/logging.hpp
  app/organicmaps/core/ScopedEnv.hpp
  app/organicmaps/core/ScopedLocalRef.hpp
  app/organicmaps/Framework.hpp
  app/organicmaps/opengl/android_gl_utils.hpp
  app/organicmaps/opengl/androidoglcontext.hpp
  app/organicmaps/opengl/androidoglcontextfactory.hpp
  app/organicmaps/opengl/gl3stub.h
  app/organicmaps/platform/GuiThread.hpp
  app/organicmaps/platform/AndroidPlatform.hpp
  app/organicmaps/util/Distance.hpp
  app/organicmaps/util/FeatureIdBuilder.hpp
  app/organicmaps/vulkan/android_vulkan_context_factory.hpp

  # JNI sources
  app/organicmaps/core/jni_helper.cpp
  app/organicmaps/core/jni_java_methods.cpp
  app/organicmaps/core/logging.cpp
  app/organicmaps/bookmarks/data/BookmarkManager.cpp
  app/organicmaps/DisplayedCategories.cpp
  app/organicmaps/DownloadResourcesLegacyActivity.cpp
  app/organicmaps/editor/Editor.cpp
  app/organicmaps/editor/OpeningHours.cpp
  app/organicmaps/editor/OsmOAuth.cpp
  app/organicmaps/Framework.cpp
  app/organicmaps/isolines/IsolinesManager.cpp
  app/organicmaps/LocationState.cpp
  app/organicmaps/Map.cpp
  app/organicmaps/MapManager.cpp
  app/organicmaps/MwmApplication.cpp
  app/organicmaps/routing/RoutingOptions.cpp
  app/organicmaps/SearchEngine.cpp
  app/organicmaps/SearchRecents.cpp
  app/organicmaps/settings/UnitLocale.cpp
  app/organicmaps/settings/MapLanguageCode.cpp
  app/organicmaps/sound/tts.cpp
  app/organicmaps/subway/SubwayManager.cpp
  app/organicmaps/TrackRecorder.cpp
  app/organicmaps/TrafficState.cpp
  app/organicmaps/UserMarkHelper.cpp
  app/organicmaps/opengl/android_gl_utils.cpp
  app/organicmaps/opengl/androidoglcontext.cpp
  app/organicmaps/opengl/androidoglcontextfactory.cpp
  app/organicmaps/opengl/gl3stub.cpp
  app/organicmaps/platform/GuiThread.cpp
  app/organicmaps/platform/HttpThread.cpp
  app/organicmaps/platform/Language.cpp
  app/organicmaps/platform/Localization.cpp
  app/organicmaps/platform/AndroidPlatform.cpp
  app/organicmaps/platform/PThreadImpl.cpp
  app/organicmaps/platform/SecureStorage.cpp
  app/organicmaps/platform/SocketImpl.cpp
  app/organicmaps/util/Config.cpp
  app/organicmaps/util/GeoUtils.cpp
  app/organicmaps/util/HttpClient.cpp
  app/organicmaps/util/Language.cpp
  app/organicmaps/util/LogsManager.cpp
  app/organicmaps/util/NetworkPolicy.cpp
  app/organicmaps/util/StringUtils.cpp
  app/organicmaps/vulkan/android_vulkan_context_factory.cpp
)

omim_add_library(${PROJECT_NAME} SHARED ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE .)

target_link_libraries(${PROJECT_NAME}
  # Organic Maps libs
  map
  # ge0
  # tracking
  # routing
  # traffic
  # routing_common
  # drape_frontend
  # shaders
  # search
  # storage
  # descriptions
  # drape
  # kml
  # editor
  # transit
  # indexer
  # platform
  # mwm_diff
  # bsdiff
  # geometry
  # coding
  # base
  # opening_hours
  # pugixml
  # expat
  # freetype
  # minizip
  # cppjansson
  # protobuf
  # succinct
  # stb_image
  # icu
  # agg
  # vulkan_wrapper

  # Android libs
  log
  android
  EGL
  GLESv2
  atomic
  z
)

#target_link_options(${PROJECT_NAME} PRIVATE "-fuse-ld=gold")
