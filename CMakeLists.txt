cmake_minimum_required(VERSION 3.18)
project(omim C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
# Our code does not rely on gnu extensions.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Fixes warning ld: warning: ignoring duplicate libraries on Mac and Windows.
if (POLICY CMP0156)
  cmake_policy(SET CMP0156 NEW)
endif()

if (APPLE AND NOT ("${CMAKE_SYSTEM_NAME}" STREQUAL Android))
  # OBJC/OBJCXX are needed to skip m/mm files in Unity builds.
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21963
  enable_language(OBJC)
  set(CMAKE_OBJC_EXTENSIONS OFF)
  set(CMAKE_OBJC_STANDARD 11)
  set(CMAKE_OBJC_FLAGS -fobjc-arc)
  set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
  enable_language(OBJCXX)
  set(CMAKE_OBJCXX_EXTENSIONS OFF)
  set(CMAKE_OBJCXX_STANDARD 17)
  set(CMAKE_OBJCXX_FLAGS -fobjc-arc)
  set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
endif()

message(STATUS "Using compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

option(COVERAGE_REPORT "Configure for coverage report" OFF)

option(UNITY_DISABLE "Disable unity build" OFF)
if (NOT UNITY_DISABLE AND NOT DEFINED ENV{UNITY_DISABLE})
  set(CMAKE_UNITY_BUILD ON)
  if (DEFINED ENV{UNITY_BUILD_BATCH_SIZE})
    set(CMAKE_UNITY_BUILD_BATCH_SIZE $ENV{UNITY_BUILD_BATCH_SIZE})
  else()
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 50)
  endif()
  message(STATUS "Using Unity Build with batch ${CMAKE_UNITY_BUILD_BATCH_SIZE}, export UNITY_DISABLE=1 or use -DUNITY_DISABLE=ON to disable it.")
endif()

option(CCACHE_DISABLE "Disable ccache" OFF)
if (NOT CCACHE_DISABLE AND NOT DEFINED ENV{CCACHE_DISABLE})
  find_program(CCACHE_PROGRAM ccache HINTS /usr/local/bin/)
  if (CCACHE_PROGRAM)
    message(STATUS "Using ccache, export CCACHE_DISABLE=1 or use -DCCACHE_DISABLE=ON to disable it.")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  endif()
endif()

option(COLORS_DISABLE "Disable colored compiler output" OFF)
if (NOT DEFINED ENV{COLORS_DISABLE} AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "export COLORS_DISABLE=1 or use -DCOLORS_DISABLE=ON to disable colored compiler output.")
  add_compile_options($<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always> $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcolor-diagnostics>)
  add_link_options($<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always> $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcolor-diagnostics>)
endif()

option(WITH_SYSTEM_PROVIDED_3PARTY "Enable compilation with system provided dependencies" OFF)
set(OMIM_ROOT ${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${OMIM_ROOT}/cmake")

include(OmimHelpers)
include(OmimTesting)

set(PLATFORM_DESKTOP TRUE)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PLATFORM_LINUX TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(PLATFORM_MAC TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PLATFORM_WIN TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(PLATFORM_ANDROID TRUE)
  set(PLATFORM_DESKTOP FALSE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(PLATFORM_IPHONE TRUE)
  set(PLATFORM_DESKTOP FALSE)
else()
  message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

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
if (MSVC)
  add_compile_options(/utf-8)
  add_link_options(/INCREMENTAL:NO)
else()
  add_compile_options(-ffast-math)
endif()

if (PLATFORM_WIN)
  add_definitions(
    -DWIN32_LEAN_AND_MEAN
    -DNOMINMAX
  )
endif()

# Built-in CMake configurations: Debug, Release, RelWithDebInfo, MinSizeRel
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  add_definitions(-DDEBUG)
  if (NOT MSVC)
    add_compile_options(-fno-omit-frame-pointer)
  endif()
elseif (${CMAKE_BUILD_TYPE} MATCHES "Rel")
  add_definitions(-DRELEASE)
  if (NOT MSVC)
    add_compile_options(-Ofast)  # Also enables -ffast-math
  endif()
else()
  message(FATAL_ERROR "Unknown build type: " ${CMAKE_BUILD_TYPE})
endif()

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
# End of setting build type

# Options

# Call `make package` after cmake to build design tool.
option(BUILD_DESIGNER "Build application as design tool" OFF)
if (BUILD_DESIGNER)
  message(STATUS "Designer tool building is enabled")
  add_definitions(-DBUILD_DESIGNER)
endif()

option(USE_ASAN "Enable Address Sanitizer" OFF)
option(USE_TSAN "Enable Thread Sanitizer" OFF)
option(USE_LIBFUZZER "Enable LibFuzzer" OFF)
option(PYBINDINGS "Create makefiles for building python bindings" OFF)
option(SKIP_QT_GUI "Skip building of Qt GUI" OFF)
option(USE_PCH "Use precompiled headers" OFF)
option(NJOBS "Number of parallel processes" OFF)
option(ENABLE_VULKAN_DIAGNOSTICS "Enable Vulkan diagnostics" OFF)

if (NJOBS)
  message(STATUS "Number of parallel processes: ${NJOBS}")
  set(CMAKE_JOB_POOLS custom=${NJOBS})
  set(CMAKE_JOB_POOL_COMPILE custom)
  set(CMAKE_JOB_POOL_LINK custom)
  set(CMAKE_JOB_POOL_PRECOMPILE_HEADER custom)
endif()

# GCC 8.1 is required to support <charconv> header inclusion in base/string_utils.hpp, otherwise 7.0 is sufficient
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.1)
  message(FATAL_ERROR "Minimum supported g++ version is 8.1 yours is ${CMAKE_CXX_COMPILER_VERSION}")
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
  message(STATUS "Address Sanitizer is enabled")
endif()

if (USE_TSAN)
  message(STATUS "Thread Sanitizer is enabled")
endif()

if (USE_ASAN AND USE_TSAN)
  message(FATAL_ERROR "Can't use two different sanitizers together")
endif()

if (USE_LIBFUZZER)
  message(STATUS "LibFuzzer is enabled")
endif()

if (USE_PPROF)
  message(STATUS "Google Profiler is enabled")
  add_definitions(-DUSE_PPROF)
endif()

if (USE_HEAPPROF)
  message(STATUS "Heap Profiler is enabled")
endif()

if (ENABLE_VULKAN_DIAGNOSTICS)
  message(WARNING "Vulkan diagnostics are enabled. Be aware of performance impact!")
  add_definitions(-DENABLE_VULKAN_DIAGNOSTICS)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set environment variables
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

if (PLATFORM_LINUX OR PLATFORM_ANDROID)
  find_program(LLD_FOUND ld.lld)
  if (LLD_FOUND)
    message(STATUS "Using ld.ldd linker")
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
  else()
    find_program(GOLD_FOUND ld.gold)
    if (GOLD_FOUND)
      message(STATUS "Using ld.gold")
      set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=gold")
    endif()
  endif()
endif()

if (NOT SKIP_TESTS)
  enable_testing()
  # Enables ctest -T memcheck with valgrind
  include(CTest)
  if (COVERAGE_REPORT)
    include(OmimCoverage)
  endif ()
endif()

if (NOT PYTHON_VERSION)
  # FDroid build server has 3.5.3 Python installed.
  set(PYTHON_VERSION 3.5)
endif()

# End of setting environment variables

find_package(Threads REQUIRED)

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
  list(APPEND qt_components Core Network)
  if (NOT SKIP_QT_GUI OR NOT SKIP_TESTS OR PYBINDINGS)
    list(APPEND qt_components Widgets)
  endif()
  if (NOT SKIP_QT_GUI)
    list(APPEND qt_components Gui Xml Svg OpenGL OpenGLWidgets)
  endif()
  # PATHS are hard-coded hints where to look for qt6 in addition to other places.
  find_package(Qt6 COMPONENTS REQUIRED ${qt_components} PATHS $ENV{QT_PATH} /opt/homebrew/opt/qt@6 /usr/local/opt/qt@6 /usr/lib/x86_64-linux-gnu/qt6)
endif()

# To allow #include "base/file_name.hpp" in all sources.
include_directories(${CMAKE_HOME_DIRECTORY})

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
  message(STATUS "Precompiled headers are ON")
  set(OMIM_PCH_TARGET_NAME "omim_pch")
  add_precompiled_headers(
    ${OMIM_ROOT}/precompiled_headers.hpp
    ${OMIM_PCH_TARGET_NAME}
  )
endif()

# Should be on the root level, not in 3party, so tests can get these dependencies.
if (PLATFORM_LINUX OR PLATFORM_WIN)
  find_package(ICU COMPONENTS uc i18n data REQUIRED)
  find_package(Freetype REQUIRED)
  find_package(harfbuzz REQUIRED)
endif()

if (WITH_SYSTEM_PROVIDED_3PARTY)
  set(GFLAGS_USE_TARGET_NAMESPACE ON)
  find_package(gflags REQUIRED)

  find_package(expat CONFIG REQUIRED)
  find_package(jansson CONFIG REQUIRED)
  find_package(pugixml REQUIRED)
  find_package(utf8cpp REQUIRED)
endif()

find_package(ZLIB REQUIRED)

# Include 3party dependencies.
add_subdirectory(3party)

# Not needed for the usual build process, but it fixes QtCreator editor,
# that doesn't see gflags/gflags.h in binary dir (gflags has tricky cmake configuration).
if (PLATFORM_DESKTOP AND NOT WITH_SYSTEM_PROVIDED_3PARTY)
  include_directories("${PROJECT_BINARY_DIR}/3party/gflags/include")
endif()

# Used in qt/ and shaders/
find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
add_subdirectory(cppjansson)
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
  omim_add_tool_subdirectory(feature_list)
  add_subdirectory(generator)
  add_subdirectory(openlr)
  add_subdirectory(poly_borders)
  omim_add_tool_subdirectory(topography_generator)
  add_subdirectory(track_analyzing)
  omim_add_tool_subdirectory(track_generator)
  if (NOT SKIP_QT_GUI)
    add_subdirectory(qt)
    omim_add_tool_subdirectory(skin_generator)
  endif()
endif()

omim_add_test_subdirectory(qt_tstfrm)

if (PLATFORM_ANDROID)
  add_subdirectory(android/organicmaps/src/main/cpp)
endif()
