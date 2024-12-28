#include "platform/trace.hpp"

#include <android/trace.h>
#include <dlfcn.h>

namespace platform
{
// Source: https://developer.android.com/topic/performance/tracing/custom-events-native
typedef void *(*ATrace_beginSection) (char const *);
typedef void *(*ATrace_endSection) (void);
typedef void *(*ATrace_setCounter) (char const *, int64_t);

class TraceImpl
{
public:
  TraceImpl()
  {
    m_lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);

    // Access the native tracing functions.
    if (m_lib != nullptr) 
    {
      // Use dlsym() to prevent crashes on devices running Android 5.1
      // (API level 22) or lower.
      m_beginSection = reinterpret_cast<ATrace_beginSection>(dlsym(m_lib, "ATrace_beginSection"));
      m_endSection = reinterpret_cast<ATrace_endSection>(dlsym(m_lib, "ATrace_endSection"));
      m_setCounter = reinterpret_cast<ATrace_setCounter>(dlsym(m_lib, "ATrace_setCounter"));
    }
  }

  ~TraceImpl()
  {
    if (m_lib != nullptr)
      dlclose(m_lib);
  }

  void BeginSection(char const * name) noexcept
  {
    if (m_beginSection != nullptr)
      m_beginSection(name);
  }

  void EndSection() noexcept
  {
    if (m_endSection != nullptr)
      m_endSection();
  }

  void SetCounter(char const * name, int64_t value) noexcept
  {
    if (m_setCounter != nullptr)
      m_setCounter(name, value);
  }

private:
  void * m_lib = nullptr;
  ATrace_beginSection m_beginSection = nullptr;
  ATrace_endSection m_endSection = nullptr;
  ATrace_setCounter m_setCounter = nullptr;
};

// static 
Trace & Trace::Instance() noexcept {
  static Trace instance;
  return instance;
}

Trace::Trace() 
  : m_impl(std::make_unique<TraceImpl>())
{}

Trace::~Trace() = default;
  
void Trace::BeginSection(char const * name) noexcept 
{
  m_impl->BeginSection(name);
}

void Trace::EndSection() noexcept 
{
  m_impl->EndSection();
}

void Trace::SetCounter(char const * name, int64_t value) noexcept
{
  m_impl->SetCounter(name, value);
}
}  // namespace platform
