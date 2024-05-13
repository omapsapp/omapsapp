#include "androidoglcontext.hpp"
#include "android_gl_utils.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/src_point.hpp"

namespace android
{

static EGLint * getContextAttributesList(bool supportedES3)
{
  static EGLint contextAttrList[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  static EGLint contextAttrListES3[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
  };
  return supportedES3 ? contextAttrListES3 : contextAttrList;
}

AndroidOGLContext::AndroidOGLContext(bool supportedES3, EGLDisplay display, EGLSurface surface,
                                     EGLConfig config, AndroidOGLContext * contextToShareWith)
  : m_nativeContext(EGL_NO_CONTEXT)
  , m_surface(surface)
  , m_display(display)
  , m_presentAvailable(true)
{
  ASSERT(m_surface != EGL_NO_SURFACE, ());
  ASSERT(m_display != EGL_NO_DISPLAY, ());

  EGLContext sharedContext = (contextToShareWith == NULL) ? EGL_NO_CONTEXT : contextToShareWith->m_nativeContext;
  m_nativeContext = eglCreateContext(m_display, config, sharedContext, getContextAttributesList(supportedES3));
  CHECK(m_nativeContext != EGL_NO_CONTEXT, ());
}

AndroidOGLContext::~AndroidOGLContext()
{
  // Native context must exist
  if (eglDestroyContext(m_display, m_nativeContext) == EGL_FALSE)
    CHECK_EGL_CALL();
}

void AndroidOGLContext::SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer)
{
  if (framebuffer)
    framebuffer->Bind();
  else
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void AndroidOGLContext::MakeCurrent()
{
  ASSERT(m_surface != EGL_NO_SURFACE, ());
  if (eglMakeCurrent(m_display, m_surface, m_surface, m_nativeContext) == EGL_FALSE)
    CHECK_EGL_CALL();
}

void AndroidOGLContext::DoneCurrent()
{
  ClearCurrent();
}

void AndroidOGLContext::ClearCurrent()
{
  if (eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE)
    CHECK_EGL_CALL();
}

void AndroidOGLContext::SetRenderingEnabled(bool enabled)
{
  if (enabled)
    MakeCurrent();
  else
    ClearCurrent();
}

void AndroidOGLContext::SetPresentAvailable(bool available)
{
  m_presentAvailable = available;
}

bool AndroidOGLContext::Validate()
{
  if (!m_presentAvailable)
    return false;
  return eglGetCurrentDisplay() != EGL_NO_DISPLAY &&
         eglGetCurrentSurface(EGL_DRAW) != EGL_NO_SURFACE &&
         eglGetCurrentContext() != EGL_NO_CONTEXT;
}

void AndroidOGLContext::Present()
{
  if (!m_presentAvailable)
    return;
  ASSERT(m_surface != EGL_NO_SURFACE, ());
  if (eglSwapBuffers(m_display, m_surface) == EGL_FALSE)
    CHECK_EGL_CALL();
}

void AndroidOGLContext::SetSurface(EGLSurface surface)
{
  m_surface = surface;
  ASSERT(m_surface != EGL_NO_SURFACE, ());
}

void AndroidOGLContext::ResetSurface()
{
  m_surface = EGL_NO_SURFACE;
}
}  // namespace android
