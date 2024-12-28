#pragma once

#include "std/target_os.hpp"

// Both clang and gcc implements `#pragma GCC`
#if !defined(__INTEL_COMPILER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  // !defined(__INTEL_COMPILER)

#include "indexer/drules_struct.pb.h"

#if !defined(__INTEL_COMPILER)
#pragma GCC diagnostic pop
#endif  // !defined(__INTEL_COMPILER)
