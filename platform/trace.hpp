#pragma once

#include "base/macros.hpp"

#include <cstdint>
#include <memory>

namespace platform
{
class TraceImpl;

// API is inspired by https://developer.android.com/ndk/reference/group/tracing
class Trace
{
public:
  static Trace & Instance() noexcept;

  void BeginSection(char const * name) noexcept;
  void EndSection() noexcept;
  void SetCounter(char const * name, int64_t value) noexcept;

private:
  Trace();
  ~Trace();

  std::unique_ptr<TraceImpl> m_impl;

  DISALLOW_COPY_AND_MOVE(Trace);
};

class TraceSection
{
public:
  inline TraceSection(char const * section) noexcept
  {
    Trace::Instance().BeginSection(section);
  }

  inline ~TraceSection() noexcept
  {
    Trace::Instance().EndSection();
  }
};
}  // namespace platform

#if defined(ENABLE_TRACE) && defined(OMIM_OS_ANDROID)
#define TRACE_SECTION(section) platform::TraceSection ___section(section)
#define TRACE_COUNTER(name, value) platform::Trace::Instance().SetCounter(name, value)
#else
#define TRACE_SECTION(section) static_cast<void>(0)
#define TRACE_COUNTER(name, value) static_cast<void>(0)
#endif
