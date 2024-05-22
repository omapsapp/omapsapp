#include "base/pprof.hpp"

#include "base/assert.hpp"

#if defined(USE_PPROF)
#include <gperftools/profiler.h>
#endif

namespace base
{
PProf::PProf(std::string const & path)
{
#if defined(USE_PPROF)
  ProfilerStart(path.c_str());
#else
  UNUSED_VALUE(path);
#endif
}

PProf::~PProf()
{
  CHECK(m_checker.CalledOnOriginalThread(), ());

#if defined(USE_PPROF)
  ProfilerStop();
#endif
}
}  // namespace base
