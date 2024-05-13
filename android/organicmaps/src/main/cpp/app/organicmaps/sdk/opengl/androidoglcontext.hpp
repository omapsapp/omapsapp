#pragma once

#include "drape/gl_includes.hpp"
#include "drape/oglcontext.hpp"

#include <atomic>

namespace android
{
class AndroidOGLContext : public dp::OGLContext
{
public:
  AndroidOGLContext(bool supportedES3, EGLDisplay display, EGLSurface surface,
                    EGLConfig config, AndroidOGLContext * contextToShareWith);
  ~AndroidOGLContext();

  void MakeCurrent() override;
  void DoneCurrent() override;
  void Present() override;
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override;
  void SetRenderingEnabled(bool enabled) override;
  void SetPresentAvailable(bool available) override;
  bool Validate() override;

  void SetSurface(EGLSurface surface);
  void ResetSurface();

  void ClearCurrent();

private:
  // {@ Owned by Context
  EGLContext m_nativeContext;
  // @}

  // {@ Owned by Factory
  EGLSurface m_surface;
  EGLDisplay m_display;
  // @}

  std::atomic<bool> m_presentAvailable;
};
}  // namespace android
