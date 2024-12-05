#include "qt/qt_common/qtoglcontext.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/math.hpp"

#include "drape/gl_functions.hpp"

#include <memory>

namespace qt
{
namespace common
{
QtRenderOGLContext::QtRenderOGLContext(QOpenGLContext * rootContext, QOffscreenSurface * surface)
  : m_surface(surface)
  , m_ctx(std::make_unique<QOpenGLContext>())
  , m_isContextAvailable(false)
{
  m_ctx->setFormat(rootContext->format());
  m_ctx->setShareContext(rootContext);
  m_ctx->create();
  CHECK(m_ctx->isValid(), ());
}

void QtRenderOGLContext::Present()
{
  GLFunctions::glFinish();

  std::lock_guard<std::mutex> lock(m_frameMutex);
  std::swap(m_frontFrame, m_backFrame);
  m_frameUpdated = true;
}

void QtRenderOGLContext::MakeCurrent()
{
  CHECK(m_ctx->makeCurrent(m_surface), ());
  m_isContextAvailable = true;
}

void QtRenderOGLContext::DoneCurrent()
{
  m_isContextAvailable = false;
  m_ctx->doneCurrent();
}

void QtRenderOGLContext::SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer)
{
  if (framebuffer)
    framebuffer->Bind();
  else if (m_backFrame != nullptr)
    m_backFrame->bind();
}

void QtRenderOGLContext::Resize(int w, int h)
{
  CHECK(m_isContextAvailable, ());
  CHECK_GREATER_OR_EQUAL(w, 0, ());
  CHECK_GREATER_OR_EQUAL(h, 0, ());

  // This function can't be called inside BeginRendering - EndRendering.
  std::lock_guard<std::mutex> lock(m_frameMutex);

  auto const nw = static_cast<int>(math::NextPowOf2(static_cast<uint32_t>(w)));
  auto const nh = static_cast<int>(math::NextPowOf2(static_cast<uint32_t>(h)));

  if (nw <= m_width && nh <= m_height && m_backFrame != nullptr)
  {
    m_frameRect =
      QRectF(0.0, 0.0, w / static_cast<float>(m_width), h / static_cast<float>(m_height));
    return;
  }

  m_width = nw;
  m_height = nh;
  m_frameRect =
    QRectF(0.0, 0.0, w / static_cast<float>(m_width), h / static_cast<float>(m_height));

  m_backFrame = std::make_unique<QOpenGLFramebufferObject>(QSize(m_width, m_height),
                                                           QOpenGLFramebufferObject::Depth);
  m_frontFrame = std::make_unique<QOpenGLFramebufferObject>(QSize(m_width, m_height),
                                                            QOpenGLFramebufferObject::Depth);
  m_acquiredFrame = std::make_unique<QOpenGLFramebufferObject>(QSize(m_width, m_height),
                                                               QOpenGLFramebufferObject::Depth);
}

bool QtRenderOGLContext::AcquireFrame()
{
  if (!m_isContextAvailable)
    return false;

  std::lock_guard<std::mutex> lock(m_frameMutex);
  // Render current acquired frame.
  if (!m_frameUpdated)
    return true;

  // Update acquired frame.
  m_acquiredFrameRect = m_frameRect;
  std::swap(m_acquiredFrame, m_frontFrame);
  m_frameUpdated = false;

  return true;
}

QRectF const & QtRenderOGLContext::GetTexRect() const
{
  return m_acquiredFrameRect;
}

GLuint QtRenderOGLContext::GetTextureHandle() const
{
  if (!m_acquiredFrame)
    return 0;

  CHECK(!m_acquiredFrame->isBound(), ());
  return m_acquiredFrame->texture();
}

QtUploadOGLContext::QtUploadOGLContext(QOpenGLContext * rootContext, QOffscreenSurface * surface)
  : m_surface(surface), m_ctx(std::make_unique<QOpenGLContext>())
{
  m_ctx->setFormat(rootContext->format());
  m_ctx->setShareContext(rootContext);
  m_ctx->create();
  CHECK(m_ctx->isValid(), ());
}

void QtUploadOGLContext::MakeCurrent()
{
  m_ctx->makeCurrent(m_surface);
}

void QtUploadOGLContext::DoneCurrent()
{
  m_ctx->doneCurrent();
}

void QtUploadOGLContext::Present()
{
  CHECK(false, ());
}

void QtUploadOGLContext::SetFramebuffer(ref_ptr<dp::BaseFramebuffer>)
{
  CHECK(false, ());
}
}  // namespace common
}  // namespace qt
