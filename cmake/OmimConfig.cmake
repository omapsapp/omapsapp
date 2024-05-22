# Flags for all
set(OMIM_WARNING_FLAGS
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
set(3PARTY_INCLUDE_DIRS "${OMIM_ROOT}/3party/boost")
set(OMIM_DATA_DIR "${OMIM_ROOT}/data")
set(OMIM_USER_RESOURCES_DIR "${OMIM_ROOT}/data")
