#pragma once

#include "app/organicmaps/core/jni_helper.hpp"
#include "androidoglcontext.hpp"
#include "drape/graphics_context_factory.hpp"

#include "base/src_point.hpp"

#include <condition_variable>
#include <mutex>

namespace android
{
class AndroidOGLContextFactory : public dp::GraphicsContextFactory
{
public:
  AndroidOGLContextFactory(JNIEnv * env, jobject jsurface);
  ~AndroidOGLContextFactory();

  bool IsValid() const;

  dp::GraphicsContext * GetDrawContext() override;
  dp::GraphicsContext * GetResourcesUploadContext() override;
  bool IsDrawContextCreated() const override;
  bool IsUploadContextCreated() const override;
  void WaitForInitialization(dp::GraphicsContext * context) override;
  void SetPresentAvailable(bool available) override;

  void SetSurface(JNIEnv * env, jobject jsurface);
  void ResetSurface();

  int GetWidth() const;
  int GetHeight() const;
  void UpdateSurfaceSize(int w, int h);

  bool IsSupportedOpenGLES3() const { return m_supportedES3; }

private:
  bool QuerySurfaceSize();

private:
  bool CreateWindowSurface();
  bool CreatePixelbufferSurface();

  AndroidOGLContext * m_drawContext;
  AndroidOGLContext * m_uploadContext;

  EGLSurface m_windowSurface;
  EGLSurface m_pixelbufferSurface;
  EGLConfig  m_config;

  ANativeWindow * m_nativeWindow;
  EGLDisplay m_display;

  int m_surfaceWidth;
  int m_surfaceHeight;

  bool m_windowSurfaceValid;
  bool m_supportedES3;

  bool m_isInitialized = false;
  size_t m_initializationCounter = 0;
  std::condition_variable m_initializationCondition;
  std::mutex m_initializationMutex;
};
}  // namespace android
