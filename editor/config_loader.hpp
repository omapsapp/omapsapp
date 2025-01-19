#pragma once

#include "base/exception.hpp"
#include "base/logging.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace pugi
{
class xml_document;
}

namespace editor
{
class EditorConfig;

// Class for multithreaded interruptable waiting.
class Waiter
{
public:
  template <typename Rep, typename Period>
  bool Wait(std::chrono::duration<Rep, Period> const & waitDuration)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_interrupted)
      return false;

    m_event.wait_for(lock, waitDuration, [this]() { return m_interrupted; });

    return true;
  }

  void Interrupt();

private:
  bool m_interrupted = false;
  std::mutex m_mutex;
  std::condition_variable m_event;
};

// Class which loads config from local drive
class ConfigLoader
{
public:
  explicit ConfigLoader(std::atomic<std::shared_ptr<EditorConfig>> & config);
  ~ConfigLoader();

  // Static methods for production and testing.
  static void LoadFromLocal(pugi::xml_document & doc);

private:
  void ResetConfig(pugi::xml_document const & doc);

  std::atomic<std::shared_ptr<EditorConfig>> & m_config;

  Waiter m_waiter;
};
}  // namespace editor
