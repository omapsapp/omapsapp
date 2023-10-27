add_compile_options(-O0 --coverage)
add_link_options(--coverage)

set(COVERAGE_REPORT_DIR ${CMAKE_BINARY_DIR}/coverage_report)

find_program(GCOVR_EXECUTABLE_PATH gcovr)
if (NOT GCOVR_EXECUTABLE_PATH)
  message(FATAL_ERROR "'gcovr' is required to generate test coverage report. Details: gcovr.com.")
endif ()

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|AppleClang")
  set(GCOV_EXECUTABLE "gcov")
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set(GCOV_EXECUTABLE "llvm-cov")
endif ()

find_program(GCOV_EXECUTABLE_PATH ${GCOV_EXECUTABLE})
if (NOT GCOV_EXECUTABLE_PATH)
  message(FATAL_ERROR "'${GCOV_EXECUTABLE}' is required to generate test coverage report.")
endif ()

if (${GCOV_EXECUTABLE_PATH} MATCHES "llvm-cov")
  set(GCOV_EXECUTABLE_PATH "${GCOV_EXECUTABLE_PATH} gcov")
endif ()

add_custom_target(omim_coverage
  # Remove harfbuzz.cc.* files because they reference .rl files that do not exist and cannot be excluded by gcovr.
  COMMAND rm -f ${CMAKE_BINARY_DIR}/3party/harfbuzz/CMakeFiles/harfbuzz.dir/harfbuzz/src/harfbuzz.cc.*
  # Recreate coverage_report folder
  COMMAND rm -rf ${COVERAGE_REPORT_DIR} && mkdir ${COVERAGE_REPORT_DIR}
  # Run gcovr
  COMMAND ${GCOVR_EXECUTABLE_PATH}
  --config=${OMIM_ROOT}/gcovr.cfg
  --root=${OMIM_ROOT}
  --object-directory=${CMAKE_BINARY_DIR}
  --exclude=${CMAKE_BINARY_DIR} # Exclude autogenerated files from Qt and some 3party libraries.
  --gcov-executable=${GCOV_EXECUTABLE_PATH}
  --html-nested=${COVERAGE_REPORT_DIR}/html/ >> ${COVERAGE_REPORT_DIR}/summary.txt
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generating coverage report..."
)
