#pragma once

#include "std/target_os.hpp"

#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif  // defined(__GNUC__) && !defined(__INTEL_COMPILER)

#include "indexer/drules_struct.pb.h"

#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) && !defined(__INTEL_COMPILER)
