# Fixes CMake deprecation warning:
# Compatibility with CMake < 3.5 will be removed from a future version of CMake.
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

if (NOT WITH_SYSTEM_PROVIDED_3PARTY)
  # Suppress "Policy CMP0077 is not set: option() honors normal variables"
  # for the freetype, expat and jansson options.
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  # Suppress "Policy CMP0063 is not set: Honor visibility properties for all target types."
  # for jansson
  set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)

  # Configure expat library.
  set(EXPAT_BUILD_TOOLS OFF)
  set(EXPAT_BUILD_EXAMPLES OFF)
  set(EXPAT_BUILD_TESTS OFF)
  set(EXPAT_BUILD_DOCS OFF)
  set(EXPAT_BUILD_PKGCONFIG OFF)
  set(EXPAT_ENABLE_INSTALL OFF)
  set(EXPAT_SHARED_LIBS OFF)
  set(EXPAT_GE OFF)
  set(EXPAT_DTD OFF)
  set(EXPAT_NS ON)
  add_subdirectory(expat/expat)

  # Configure Jansson library.
  set(JANSSON_BUILD_DOCS OFF)
  set(JANSSON_BUILD_MAN OFF)
  set(JANSSON_EXAMPLES OFF)
  set(JANSSON_INSTALL OFF)
  set(JANSSON_WITHOUT_TESTS ON)
  add_subdirectory(jansson/jansson/)
  target_include_directories(jansson INTERFACE "${PROJECT_BINARY_DIR}/3party/jansson/jansson/include")
  add_library(jansson::jansson ALIAS jansson)

  # Add gflags library.
  add_subdirectory(gflags)
  target_compile_options(gflags_nothreads_static PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-subobject-linkage>)

  # Add pugixml library.
  add_subdirectory(pugixml)

  # Add protobuf library.
  add_subdirectory(protobuf)

  if (NOT PLATFORM_LINUX)
    add_subdirectory(freetype)
    add_subdirectory(icu)
    add_subdirectory(harfbuzz)
  endif()

  add_library(utf8cpp INTERFACE)
  add_library(utf8cpp::utf8cpp ALIAS utf8cpp)
  target_include_directories(utf8cpp INTERFACE "${OMIM_ROOT}/3party/utfcpp/source")
endif()

add_subdirectory(agg)
add_subdirectory(bsdiff-courgette)
add_subdirectory(minizip)
add_subdirectory(open-location-code)
add_subdirectory(opening_hours)
add_subdirectory(stb_image)
add_subdirectory(succinct)

add_subdirectory(vulkan_wrapper)

if (PLATFORM_DESKTOP)
  add_subdirectory(libtess2)
endif()
