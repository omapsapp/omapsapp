include(OmimConfig)

# Functions for using in subdirectories
function(omim_add_executable executable)
  add_executable(${executable} ${ARGN})

  if (PLATFORM_WIN)
    target_sources(${executable} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/windows/OrganicMaps.manifest")
  endif()

  # Enable warnings for all our binaries.
  target_compile_options(${executable} PRIVATE ${OMIM_WARNING_FLAGS})
  target_include_directories(${executable} SYSTEM PRIVATE ${3PARTY_INCLUDE_DIRS})
  if (USE_ASAN)
    target_link_libraries(${executable}
      -fsanitize=address
      -fno-omit-frame-pointer
    )
  endif()
  if (USE_TSAN)
    target_link_libraries(${executable}
      -fsanitize=thread
      -fno-omit-frame-pointer
    )
  endif()
  if (USE_LIBFUZZER)
    target_link_libraries(${executable} -fsanitize=fuzzer)
  endif()
  if (USE_PPROF)
    if (PLATFORM_MAC)
      find_library(PPROF_LIBRARY libprofiler.dylib)
      target_link_libraries(${executable} ${PPROF_LIBRARY})
    else()
      target_link_libraries(${executable} -lprofiler)
    endif()
  endif()
  if (USE_HEAPPROF)
    if (PLATFORM_MAC)
      find_library(HEAPPROF_LIBRARY libtcmalloc.dylib)
      if (NOT HEAPPROF_LIBRARY)
          message(FATAL_ERROR
            "Trying to use -ltcmalloc on MacOS, make sure that you have installed it (https://github.com/mapsme/omim/pull/12671).")
      endif()
      target_link_libraries(${executable} ${HEAPPROF_LIBRARY})
    else()
      target_link_libraries(${executable} -ltcmalloc)
    endif()
  endif()
  if (USE_PCH)
    add_precompiled_headers_to_target(${executable} ${OMIM_PCH_TARGET_NAME})
  endif()
endfunction()

function(omim_add_library library)
  add_library(${library} ${ARGN})

  # Enable warnings for all our libraries.
  target_compile_options(${library} PRIVATE ${OMIM_WARNING_FLAGS})
  target_include_directories(${library} SYSTEM PRIVATE ${3PARTY_INCLUDE_DIRS})
  if (USE_PPROF AND PLATFORM_MAC)
    find_path(PPROF_INCLUDE_DIR NAMES gperftools/profiler.h)
    target_include_directories(${library} SYSTEM PUBLIC ${PPROF_INCLUDE_DIR})
  endif()
  if (USE_PCH)
    add_precompiled_headers_to_target(${library} ${OMIM_PCH_TARGET_NAME})
  endif()
endfunction()

function(omim_add_tool_subdirectory subdir)
  if (NOT SKIP_TOOLS)
    add_subdirectory(${subdir})
  else()
    message("SKIP_TOOLS: Skipping subdirectory ${subdir}")
  endif()
endfunction()

function(omim_add_pybindings_subdirectory subdir)
  if (PYBINDINGS)
    add_subdirectory(${subdir})
  else()
    message("Skipping pybindings subdirectory ${subdir}")
  endif()
endfunction()

function(omim_link_platform_deps target)
  if ("${ARGN}" MATCHES "platform")
    if (PLATFORM_MAC)
      target_link_libraries(
        ${target}
        "-framework CFNetwork"
        "-framework Foundation"
        "-framework IOKit"
        "-framework SystemConfiguration"
        "-framework Security"
      )
    endif()
  endif()
endfunction()

function(omim_link_libraries target)
  if (TARGET ${target})
    target_link_libraries(${target} ${ARGN} ${CMAKE_THREAD_LIBS_INIT})
    omim_link_platform_deps(${target} ${ARGN})
  else()
    message("~> Skipping linking the libraries to the target ${target} as it"
            " does not exist")
  endif()
endfunction()

function(append VAR)
  set(${VAR} ${${VAR}} ${ARGN} PARENT_SCOPE)
endfunction()

function(export_directory_flags filename)
  get_directory_property(include_directories INCLUDE_DIRECTORIES)
  get_directory_property(definitions COMPILE_DEFINITIONS)
  get_directory_property(flags COMPILE_FLAGS)
  get_directory_property(options COMPILE_OPTIONS)

  if (PLATFORM_ANDROID)
    set(
      include_directories
      ${include_directories}
      ${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES}
    )
    set(
      platform_flags
      "${ANDROID_COMPILER_FLAGS} ${ANDROID_COMPILER_FLAGS_CXX}"
    )
    set(
      flags
      "--target=${CMAKE_C_COMPILER_TARGET}"
      "--sysroot=${CMAKE_SYSROOT}" ${flags}
    )
  endif()

  # Append Release/Debug flags:
  string(TOUPPER "${CMAKE_BUILD_TYPE}" upper_build_type)
  set(flags ${flags} ${CMAKE_CXX_FLAGS_${upper_build_type}})

  set(
    include_directories
    "$<$<BOOL:${include_directories}>\:-I$<JOIN:${include_directories},\n-I>\n>"
  )
  set(definitions "$<$<BOOL:${definitions}>:-D$<JOIN:${definitions},\n-D>\n>")
  set(flags "$<$<BOOL:${flags}>:$<JOIN:${flags},\n>\n>")
  set(options "$<$<BOOL:${options}>:$<JOIN:${options},\n>\n>")
  file(
    GENERATE OUTPUT
    ${filename}
    CONTENT
    "${definitions}${include_directories}${platform_flags}\n${flags}${options}\n"
  )
endfunction()

function(add_pic_pch_target header pch_target_name
         pch_file_name suffix pic_flag)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/pch_${suffix}")
  file(COPY "${header}" DESTINATION "${CMAKE_BINARY_DIR}/pch_${suffix}")
  set(_header "${CMAKE_BINARY_DIR}/pch_${suffix}/${pch_file_name}")
  set(
    _compiled_header
    "${CMAKE_BINARY_DIR}/pch_${suffix}/${pch_file_name}.${PCH_EXTENSION}"
  )
  add_custom_target(
    "${pch_target_name}_${suffix}"
    COMMAND
    "${CMAKE_CXX_COMPILER}" ${compiler_flags} ${c_standard_flags} ${pic_flag}
                            -x c++-header
                            -c "${_header}" -o "${_compiled_header}"
    COMMENT "Building precompiled omim CXX ${suffix} header"
  )
endfunction()

function(add_precompiled_headers header pch_target_name)
  set(pch_flags_file "${CMAKE_BINARY_DIR}/${pch_target_name}_flags_file")
  export_directory_flags("${pch_flags_file}")
  set(compiler_flags "@${pch_flags_file}")

  # CMAKE_CXX_STANDARD 20 flags:
  set(c_standard_flags "-std=c++20")
  get_filename_component(pch_file_name ${header} NAME)

  add_pic_pch_target(${header} ${pch_target_name} ${pch_file_name} lib "-fPIC")
  add_pic_pch_target(${header} ${pch_target_name} ${pch_file_name} exe "-fPIE")

  add_custom_target(
    "${pch_target_name}"
    COMMENT "Waiting for both lib and exe precompiled headers to build"
    DEPENDS "${pch_target_name}_lib" "${pch_target_name}_exe"
  )
  set_target_properties(
    ${pch_target_name}
    PROPERTIES
    PCH_NAME
    "${pch_file_name}"
  )
endfunction()

function(add_precompiled_headers_to_target target pch_target)
  add_dependencies(${target} "${pch_target}")
  get_property(sources TARGET ${target} PROPERTY SOURCES)
  get_target_property(target_type ${target} TYPE)
  get_target_property(pch_file_name ${pch_target} PCH_NAME)

  if (target_type STREQUAL "EXECUTABLE")
    set(include_compiled_header_dir "${CMAKE_BINARY_DIR}/pch_exe")
    # CMake automatically adds additional compile options after linking.
    # For example '-fPIC' flag on skin_generator_tool, because it is linked to Qt libs.
    # We force correct flag for executables.
    set(additional_clang_flags "-fPIE")
  endif()

  if (target_type MATCHES "LIBRARY")
    set(include_compiled_header_dir "${CMAKE_BINARY_DIR}/pch_lib")
  endif()

  # Force gcc first search gch header in pch_exe/pch_lib:
  target_include_directories(${target}
    BEFORE
    PUBLIC
      ${include_compiled_header_dir}
  )

  foreach(source ${sources})
    if(source MATCHES \\.\(cc|cpp|h|hpp\)$)
      if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_source_files_properties(
          ${source}
          PROPERTIES
          COMPILE_FLAGS
          "${additional_clang_flags} -include-pch \
${include_compiled_header_dir}/${pch_file_name}.${PCH_EXTENSION}"
        )
      endif()
      if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set_source_files_properties(
          ${source}
          PROPERTIES
          COMPILE_FLAGS "-include ${pch_file_name}"
        )
      endif()
    endif()
  endforeach()
endfunction()
