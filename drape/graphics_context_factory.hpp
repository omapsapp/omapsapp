#pragma once

#include "drape/graphics_context.hpp"

#include "base/assert.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>

namespace dp
{
class GraphicsContextFactory
{
public:
  virtual ~GraphicsContextFactory() = default;
  virtual GraphicsContext * GetDrawContext() = 0;
  virtual GraphicsContext * GetResourcesUploadContext() = 0;
  virtual bool IsDrawContextCreated() const { return false; }
  virtual bool IsUploadContextCreated() const { return false; }
  virtual void WaitForInitialization(dp::GraphicsContext * context) {}
  virtual void SetPresentAvailable(bool available) {}
};

class ThreadSafeFactory : public GraphicsContextFactory
{
public:
  ThreadSafeFactory(GraphicsContextFactory * factory, bool enableSharing = true);
  ~ThreadSafeFactory();
  GraphicsContext * GetDrawContext() override;
  GraphicsContext * GetResourcesUploadContext() override;

  template<typename T>
  T * CastFactory()
  {
    return dynamic_cast<T *>(m_factory);
  }

  void WaitForInitialization(dp::GraphicsContext * context) override;
  void SetPresentAvailable(bool available) override;

protected:
  using TCreateCtxFn = std::function<GraphicsContext * ()>;
  using TIsSeparateCreatedFn = std::function<bool()>;
  GraphicsContext * CreateContext(TCreateCtxFn const & createFn,
                                  TIsSeparateCreatedFn const & checkFn);

private:
  GraphicsContextFactory * m_factory;
  std::mutex m_condLock;
  std::condition_variable m_Cond;
  bool m_enableSharing;
};
}  // namespace dp
