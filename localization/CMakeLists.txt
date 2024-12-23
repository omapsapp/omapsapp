project(localization)

set(DATA_STRINGS_DIR ${OMIM_ROOT}/data/strings/)
set(GENERATOR ${PROJECT_SOURCE_DIR}/generator)
set(STRINGS_TO_GENERATE ${GENERATOR}/strings_to_generate.txt)
set(GENERATOR_TEMPLATE_FILE ${GENERATOR}/localizations.hpp.in)
set(OUTPUT_FOLDER ${PROJECT_BINARY_DIR}/generated)
set(GENERATOR_OUTPUT_FILE ${OUTPUT_FOLDER}/localization/localization.hpp)

add_custom_command(
  OUTPUT ${GENERATOR_OUTPUT_FILE}
  COMMAND Python3::Interpreter ${GENERATOR}/generate_core_localizations.py
          --template ${GENERATOR_TEMPLATE_FILE}
          --strings-dir ${DATA_STRINGS_DIR}
          --generate ${STRINGS_TO_GENERATE}
          --output ${GENERATOR_OUTPUT_FILE}
  DEPENDS ${DATA_STRINGS_DIR}
  DEPENDS ${GENERATOR}
  COMMENT "Generating core localizations"
)

set(SRC strings_bundle.hpp strings_bundle.cpp)

omim_add_library(${PROJECT_NAME} ${SRC} ${GENERATOR_OUTPUT_FILE})
target_include_directories(${PROJECT_NAME} PUBLIC ${OUTPUT_FOLDER})
