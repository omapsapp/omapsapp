#include "qt_tstfrm/test_main_loop.hpp"

#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>

#include "base/scope_guard.hpp"

#include <cstring>
#include <memory>
#include <vector>

namespace
{
class MyWidget : public QWidget
{
public:
  explicit MyWidget(testing::RenderFunction && fn)
    : m_fn(std::move(fn))
  {}

protected:
  void paintEvent(QPaintEvent *) override
  {
    m_fn(this);
  }

private:
  testing::RenderFunction m_fn;
};
}  // namespace

void RunTestLoop(char const * testName, testing::RenderFunction && fn, bool autoExit)
{
  std::vector<char> buf(strlen(testName) + 1);
  strcpy(buf.data(), testName);
  char * raw = buf.data();

  int argc = 1;
  QApplication app(argc, &raw);

  if (autoExit)
    QTimer::singleShot(3000, &app, SLOT(quit()));

  auto widget = std::make_unique<MyWidget>(std::move(fn));
  widget->setWindowTitle(testName);
  widget->show();

  app.exec();
}

void RunTestInOpenGLOffscreenEnvironment(char const * testName, bool apiOpenGLES3,
                                         testing::TestFunction const & fn)
{
  std::vector<char> buf(strlen(testName) + 1);
  strcpy(buf.data(), testName);
  char * raw = buf.data();

  int argc = 1;
  QApplication app(argc, &raw);

  QSurfaceFormat fmt;
  fmt.setAlphaBufferSize(8);
  fmt.setBlueBufferSize(8);
  fmt.setGreenBufferSize(8);
  fmt.setRedBufferSize(8);
  fmt.setStencilBufferSize(0);
  fmt.setSamples(0);
  fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  fmt.setSwapInterval(1);
  fmt.setDepthBufferSize(16);
  if (apiOpenGLES3)
  {
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setVersion(3, 2);
  }
  else
  {
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setVersion(2, 1);
  }

  auto surface = std::make_unique<QOffscreenSurface>();
  surface->setFormat(fmt);
  surface->create();

  auto context = std::make_unique<QOpenGLContext>();
  context->setFormat(fmt);
  context->create();
  context->makeCurrent(surface.get());

  if (fn)
    fn(apiOpenGLES3);

  context->doneCurrent();
  surface->destroy();

  QTimer::singleShot(0, &app, SLOT(quit()));
  app.exec();
}
