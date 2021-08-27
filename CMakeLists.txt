cmake_minimum_required(VERSION 3.18)

project(omim C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
# Our code does not rely on gnu extensions.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)

# Force colored errors and warnings for Ninja.
if (CMAKE_GENERATOR MATCHES "Ninja" AND NOT (ENV{COLORS_DISABLE} EQUAL "1") )
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-fdiagnostics-color=always)
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
          "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    add_compile_options (-fcolor-diagnostics)
  endif()
endif()

get_filename_component(OMIM_ROOT . ABSOLUTE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${OMIM_ROOT}/cmake")

include(OmimHelpers)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(LINUX_DETECTED TRUE)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Android")
  set(ANDROID_DETECTED TRUE)
endif()

if (ANDROID_DETECTED AND (${OS} MATCHES "mac"))
  set(DARWIN TRUE)
endif()

omim_set_platform_var(PLATFORM_IPHONE "iphone-.*")
omim_set_platform_var(PLATFORM_ANDROID "android-.*" ${ANDROID_DETECTED})
omim_set_platform_var(PLATFORM_MAC "macx-.*" ${APPLE})
omim_set_platform_var(PLATFORM_WIN "win32-.*" ${WIN32})
omim_set_platform_var(PLATFORM_LINUX "linux-.*" ${LINUX_DETECTED})

if (PLATFORM_LINUX OR PLATFORM_MAC OR PLATFORM_WIN)
  set(PLATFORM_DESKTOP TRUE)
else()
  set(PLATFORM_DESKTOP FALSE)
endif()

# End of setting the target platform

# Sanitizer
if (PLATFORM_DESKTOP)
  # https://clang.llvm.org/docs/UsersManual.html#controlling-code-generation
  set(BUILD_WITH_SANITIZER None CACHE STRING "Set to 'address' or others to enable sanitizer")

  if (NOT ${BUILD_WITH_SANITIZER} MATCHES "None")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${BUILD_WITH_SANITIZER} -fno-omit-frame-pointer")
      set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=${BUILD_WITH_SANITIZER} -fno-omit-frame-pointer")
      message(STATUS "Enable sanitizer: ${BUILD_WITH_SANITIZER}")
  endif()
endif()

# Set build type:
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# Global compile options for all configurations.
add_compile_options(-ffast-math)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  add_definitions(-DDEBUG)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
  add_definitions(-DRELEASE)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
  add_definitions(-DRELEASE)
  add_compile_options(
    "-fno-omit-frame-pointer"
  )
else()
  message(FATAL_ERROR "Unknown build type: " ${CMAKE_BUILD_TYPE})
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})
# End of setting build type

# Options

# Call `make package` after cmake to build design tool.
option(BUILD_DESIGNER "Build application as design tool" OFF)
if (BUILD_DESIGNER)
  message("Designer tool building is enabled")
  add_definitions(-DBUILD_DESIGNER)
endif()

option(USE_ASAN "Enable Address Sanitizer" OFF)
option(USE_TSAN "Enable Thread Sanitizer" OFF)
option(USE_LIBFUZZER "Enable LibFuzzer" OFF)
option(PYBINDINGS "Create makefiles for building python bindings" OFF)
option(SKIP_DESKTOP "Skip building of desktop application" OFF)
option(BUILD_MAPSHOT "Build mapshot tool" OFF)
option(USE_PCH "Use precompiled headers" OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
  message(FATAL_ERROR "Minimum supported g++ version is 7.0 yours is ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(PCH_EXTENSION "pch")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(PCH_EXTENSION "gch")
endif()

if (PLATFORM_LINUX)
  option(USE_PPROF "Enable Google Profiler" OFF)
endif()

if (USE_ASAN)
  message("Address Sanitizer is enabled")
endif()

if (USE_TSAN)
  message("Thread Sanitizer is enabled")
endif()

if (USE_ASAN AND USE_TSAN)
  message(FATAL_ERROR "Can't use two different sanitizers together")
endif()

if (USE_LIBFUZZER)
  message("LibFuzzer is enabled")
endif()

if (USE_PPROF)
  message("Google Profiler is enabled")
  add_definitions(-DUSE_PPROF)
endif()

if (USE_HEAPPROF)
  message("Heap Profiler is enabled")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set environment variables
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

if (NOT (ENV{CCACHE_DISABLE} EQUAL "1"))
  find_program(CCACHE_PROGRAM ccache HINTS /usr/local/bin/)
  if (CCACHE_PROGRAM)
    message(STATUS "Using ccache, export CCACHE_DISABLE=1 to disable it.")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  endif()
endif()

if (PLATFORM_LINUX)
  find_program(GOLD_FOUND ld.gold)
  if (GOLD_FOUND)
    message(STATUS "Using ld.gold")
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=gold")
  endif()
endif()

if (DEFINED ENV{QT_PATH})
  message("Qt path is set to: " $ENV{QT_PATH})
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} $ENV{QT_PATH})
else()
  file(GLOB QT_PATH_LIST "/usr/local/opt/qt*" "/usr/lib/x86_64-linux-gnu/qt*")
  if (QT_PATH_LIST)
    list(GET QT_PATH_LIST 0 QT_PATH)
    message("Found Qt path: " ${QT_PATH})
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QT_PATH})
  endif()
endif()

if (NOT SKIP_TESTS)
  set(SKIP_TESTS FALSE)
endif()

if (NOT PYTHON_VERSION)
  set(PYTHON_VERSION 2.7)
endif()

# End of setting environment variables

find_package(Threads REQUIRED)

# Cannot call it at the beginning of the file because it depends on PYTHON_VERSION.
include(BundledBoost)

# Scripts

if (NOT CMAKE_HOST_WIN32)
  execute_process(
      COMMAND "${OMIM_ROOT}/tools/unix/check_cert.sh"
      RESULT_VARIABLE CheckCertResult
  )
  if (CheckCertResult)
      message(FATAL_ERROR "Certificate check failed")
  endif()
endif()

if (NOT PLATFORM_IPHONE AND NOT PLATFORM_ANDROID)
  find_package(Qt5Core)
  if (NOT Qt5Core_FOUND)
    message(FATAL_ERROR "Qt5 cmake files were not found, please set QT_PATH environment variable")
  endif()
  if (Qt5Core_VERSION VERSION_LESS 5.5.0)
    message(FATAL_ERROR "Minimum supported Qt5 version is 5.5")
  endif()
  find_package(Qt5Network REQUIRED)
  if (NOT SKIP_DESKTOP OR NOT SKIP_TESTS OR PYBINDINGS)
    find_package(Qt5Widgets)
  endif()
  if (NOT SKIP_DESKTOP)
    find_qt5_desktop_package(Qt5Gui)
    find_qt5_desktop_package(Qt5Xml)
    find_qt5_desktop_package(Qt5Svg)
  endif()
endif()

if (PLATFORM_LINUX)
  find_package(OpenGL)
endif()

find_library(LIBZ NAMES z)
if (LIBZ STREQUAL "LIBZ-NOTFOUND")
  message(FATAL_ERROR "Failed to find libz library.")
endif()

if (NOT DEVELOPER_FRAMEWORKS_DIR)
  message("Doing nothing, because we know nothing about developer frameworks dir")
  #do nothing
else()
  include_directories(${DEVELOPER_FRAMEWORKS_DIR})
endif()

include_directories(
  ${CMAKE_HOME_DIRECTORY}
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5Network_INCLUDE_DIRS}
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (USE_ASAN)
  add_compile_options(
    "-fsanitize=address"
    "-fno-omit-frame-pointer"
  )
endif()

if (USE_TSAN)
  add_compile_options(
    "-fsanitize=thread"
    "-fno-omit-frame-pointer"
  )
endif()

if (USE_LIBFUZZER)
  add_compile_options(
    "-fsanitize=fuzzer"
  )
endif()

if (USE_PCH)
  message("Precompiled headers are ON")
  set(OMIM_PCH_TARGET_NAME "omim_pch")
  add_precompiled_headers(
    ${OMIM_ROOT}/precompiled_headers.hpp
    ${OMIM_PCH_TARGET_NAME}
  )
endif()

# Suppress "Policy CMP0077 is not set: option() honors normal variables"
# for the option(EXPAT_SHARED_LIBS) in 3party/expat/expat/CMakeLists.txt
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
# Build libexpat.a instead of libexpat.so
set(EXPAT_SHARED_LIBS OFF)

# Include subdirectories
add_subdirectory(3party/agg)
add_subdirectory(3party/bsdiff-courgette)
add_subdirectory(3party/expat/expat)
add_subdirectory(3party/freetype)
add_subdirectory(3party/gflags)
add_subdirectory(3party/icu)
add_subdirectory(3party/jansson)
add_subdirectory(3party/liboauthcpp)
add_subdirectory(3party/minizip)
add_subdirectory(3party/opening_hours)
add_subdirectory(3party/protobuf)
add_subdirectory(3party/pugixml)
add_subdirectory(3party/sdf_image)
add_subdirectory(3party/stb_image)
add_subdirectory(3party/succinct)
add_subdirectory(3party/open-location-code)
add_subdirectory(3party/vulkan_wrapper)

if (PLATFORM_DESKTOP)
  add_subdirectory(3party/libtess2)
endif()

# Only options related to warnings should be placed here.
# Other options should be set before all add_subdirectory calls.
add_compile_options(
  "-Wall"
)

add_clang_compile_options("-Wshorten-64-to-32")
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
  add_gcc_cpp_compile_options("-Wno-noexcept-type")
endif()

find_package(PythonInterp ${PYTHON_VERSION})
if (PYTHONINTERP_FOUND)
  message("Found python to use in qt/, shaders/ and 3party/: ${PYTHON_EXECUTABLE}")
else()
  message(FATAL_ERROR "Could not find python to use in qt/, shaders/ and 3party/.")
endif()

add_subdirectory(base)
add_subdirectory(coding)
add_subdirectory(descriptions)
add_subdirectory(drape)
add_subdirectory(drape_frontend)
add_subdirectory(editor)
add_subdirectory(ge0)
add_subdirectory(generator/mwm_diff)
add_subdirectory(geometry)
add_subdirectory(indexer)
add_subdirectory(kml)
add_subdirectory(map)
add_subdirectory(platform)
add_subdirectory(routing)
add_subdirectory(routing_common)
add_subdirectory(search)
add_subdirectory(shaders)
add_subdirectory(storage)
add_subdirectory(tracking)
add_subdirectory(traffic)
add_subdirectory(transit)

if (PLATFORM_DESKTOP)
  if (BUILD_MAPSHOT)
    add_subdirectory(mapshot)
    add_subdirectory(software_renderer)
  endif()
  add_subdirectory(feature_list)
  add_subdirectory(generator)
  add_subdirectory(openlr)
  add_subdirectory(poly_borders)
  add_subdirectory(topography_generator)
  add_subdirectory(track_analyzing)
  add_subdirectory(track_generator)
  if (NOT SKIP_DESKTOP)
    add_subdirectory(qt)
    add_subdirectory(skin_generator)
  endif()
endif()

omim_add_test_subdirectory(3party/gmock)
omim_add_test_subdirectory(qt_tstfrm)

if (PLATFORM_ANDROID)
  add_subdirectory(android/jni)
endif()

add_custom_target(BuildVersion ALL
  COMMAND ${CMAKE_COMMAND}
  ARGS
  -D MAPSME_CURRENT_PROJECT_ROOT=${OMIM_ROOT}
  -D PATH_WITH_BUILD_VERSION_HPP=${OMIM_ROOT}
  -D PROJECT_NAME=${PROJECT_NAME}
  -P ${OMIM_ROOT}/cmake/BuildVersion.cmake
)
