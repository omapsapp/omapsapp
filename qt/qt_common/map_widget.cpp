#include "qt/qt_common/map_widget.hpp"

#include "qt/qt_common/helpers.hpp"
#include "qt/qt_common/scale_slider.hpp"

#include "map/framework.hpp"

#include "routing/maxspeeds.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <functional>
#include <string>

#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QTouchEvent>

#include <QtGui/QMouseEvent>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QAction>
#include <QtWidgets/QMenu>

// Fraction of the viewport for a move event
static constexpr float kViewportFractionRoughMove = 0.2;

// Fraction of the viewport for a small move event
static constexpr float kViewportFractionSmoothMove = 0.1;

namespace qt::common
{
//#define ENABLE_AA_SWITCH

MapWidget::MapWidget(Framework & framework, bool isScreenshotMode, QWidget * parent)
  : QOpenGLWidget(parent)
  , m_framework(framework)
  , m_screenshotMode(isScreenshotMode)
  , m_slider(nullptr)
  , m_sliderState(SliderState::Released)
  , m_ratio(1.0f)
  , m_contextFactory(nullptr)
{
  setMouseTracking(true);
  // Update widget's content frequency is 60 fps.
  m_updateTimer = std::make_unique<QTimer>(this);
  VERIFY(connect(m_updateTimer.get(), SIGNAL(timeout()), this, SLOT(update())), ());
  m_updateTimer->setSingleShot(false);
  m_updateTimer->start(1000 / 60);
  setAttribute(Qt::WA_AcceptTouchEvents);
}

MapWidget::~MapWidget()
{
  m_framework.EnterBackground();
  m_framework.SetRenderingDisabled(true);
  m_contextFactory->PrepareToShutdown();
  m_framework.DestroyDrapeEngine();
  m_contextFactory.reset();
}

void MapWidget::BindHotkeys(QWidget & parent)
{
  Hotkey const hotkeys[] = {
      {Qt::Key_Equal, SLOT(ScalePlus())},
      {Qt::Key_Plus, SLOT(ScalePlus())},
      {Qt::Key_Minus, SLOT(ScaleMinus())},
      {Qt::Key_Right, SLOT(MoveRight())},
      {Qt::Key_Left, SLOT(MoveLeft())},
      {Qt::Key_Up, SLOT(MoveUp())},
      {Qt::Key_Down, SLOT(MoveDown())},
      {Qt::ALT | Qt::Key_Equal, SLOT(ScalePlusLight())},
      {Qt::ALT | Qt::Key_Plus, SLOT(ScalePlusLight())},
      {Qt::ALT | Qt::Key_Minus, SLOT(ScaleMinusLight())},
      {Qt::ALT | Qt::Key_Right, SLOT(MoveRightSmooth())},
      {Qt::ALT | Qt::Key_Left, SLOT(MoveLeftSmooth())},
      {Qt::ALT | Qt::Key_Up, SLOT(MoveUpSmooth())},
      {Qt::ALT | Qt::Key_Down, SLOT(MoveDownSmooth())},
#ifdef ENABLE_AA_SWITCH
      {Qt::ALT | Qt::Key_A, SLOT(AntialiasingOn())},
      {Qt::ALT | Qt::Key_S, SLOT(AntialiasingOff())},
#endif
  };

  for (auto const & hotkey : hotkeys)
  {
    auto action = std::make_unique<QAction>(&parent);
    action->setShortcut(QKeySequence(hotkey.m_key));
    connect(action.get(), SIGNAL(triggered()), this, hotkey.m_slot);
    parent.addAction(action.release());
  }
}

void MapWidget::BindSlider(ScaleSlider & slider)
{
  m_slider = &slider;

  connect(m_slider, &QAbstractSlider::actionTriggered, this, &MapWidget::ScaleChanged);
  connect(m_slider, &QAbstractSlider::sliderPressed, this, &MapWidget::SliderPressed);
  connect(m_slider, &QAbstractSlider::sliderReleased, this, &MapWidget::SliderReleased);
}

void MapWidget::CreateEngine()
{
  Framework::DrapeCreationParams p;

  p.m_apiVersion = m_apiOpenGLES3 ? dp::ApiVersion::OpenGLES3 : dp::ApiVersion::OpenGLES2;

  p.m_surfaceWidth = m_screenshotMode ? width() : static_cast<int>(m_ratio * width());
  p.m_surfaceHeight = m_screenshotMode ? height() : static_cast<int>(m_ratio * height());
  p.m_visualScale = static_cast<float>(m_ratio);
  p.m_hints.m_screenshotMode = m_screenshotMode;

  m_skin.reset(new gui::Skin(gui::ResolveGuiSkinFile("default"), m_ratio));
  m_skin->Resize(p.m_surfaceWidth, p.m_surfaceHeight);
  m_skin->ForEach(
      [&p](gui::EWidget widget, gui::Position const & pos) { p.m_widgetsInitInfo[widget] = pos; });

  p.m_widgetsInitInfo[gui::WIDGET_SCALE_FPS_LABEL] = gui::Position(dp::LeftTop);

  m_framework.CreateDrapeEngine(make_ref(m_contextFactory), std::move(p));
  m_framework.SetViewportListener(std::bind(&MapWidget::OnViewportChanged, this, std::placeholders::_1));
}

void MapWidget::ScalePlus() { m_framework.Scale(Framework::SCALE_MAG, true); }

void MapWidget::ScaleMinus() { m_framework.Scale(Framework::SCALE_MIN, true); }

void MapWidget::ScalePlusLight() { m_framework.Scale(Framework::SCALE_MAG_LIGHT, true); }

void MapWidget::ScaleMinusLight() { m_framework.Scale(Framework::SCALE_MIN_LIGHT, true); }

void MapWidget::MoveRight() { m_framework.Move(-kViewportFractionRoughMove, 0, true); }

void MapWidget::MoveRightSmooth() { m_framework.Move(-kViewportFractionSmoothMove, 0, true); }

void MapWidget::MoveLeft() { m_framework.Move(kViewportFractionRoughMove, 0, true); }

void MapWidget::MoveLeftSmooth() { m_framework.Move(kViewportFractionSmoothMove, 0, true); }

void MapWidget::MoveUp() { m_framework.Move(0, -kViewportFractionRoughMove, true); }

void MapWidget::MoveUpSmooth() { m_framework.Move(0, -kViewportFractionSmoothMove, true); }

void MapWidget::MoveDown() { m_framework.Move(0, kViewportFractionRoughMove, true); }

void MapWidget::MoveDownSmooth() { m_framework.Move(0, kViewportFractionSmoothMove, true); }

void MapWidget::AntialiasingOn()
{
  auto engine = m_framework.GetDrapeEngine();
  if (engine != nullptr)
    engine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing, true);
}

void MapWidget::AntialiasingOff()
{
  auto engine = m_framework.GetDrapeEngine();
  if (engine != nullptr)
    engine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing, false);
}

void MapWidget::ScaleChanged(int action)
{
  if (!m_slider)
    return;

  if (action == QAbstractSlider::SliderNoAction)
    return;

  double const factor = m_slider->GetScaleFactor();
  if (factor != 1.0)
    m_framework.Scale(factor, false);
}

void MapWidget::SliderPressed() { m_sliderState = SliderState::Pressed; }

void MapWidget::SliderReleased() { m_sliderState = SliderState::Released; }

m2::PointD MapWidget::GetDevicePoint(QMouseEvent * e) const
{
  return m2::PointD(L2D(e->position().x()), L2D(e->position().y()));
}

df::Touch MapWidget::GetDfTouchFromQMouseEvent(QMouseEvent * e) const
{
  df::Touch touch;
  touch.m_id = 0;
  touch.m_location = GetDevicePoint(e);
  return touch;
}

df::TouchEvent MapWidget::GetDfTouchEventFromQMouseEvent(QMouseEvent * e, df::TouchEvent::ETouchType type) const
{
  df::TouchEvent event;
  event.SetTouchType(type);
  event.SetFirstTouch(GetDfTouchFromQMouseEvent(e));
  if (IsCommandModifier(e))
    event.SetSecondTouch(GetSymmetrical(event.GetFirstTouch()));

  return event;
}

df::Touch MapWidget::GetSymmetrical(df::Touch const & touch) const
{
  m2::PointD const pixelCenter = m_framework.GetVisiblePixelCenter();
  m2::PointD const symmetricalLocation = pixelCenter + pixelCenter - m2::PointD(touch.m_location);

  df::Touch result;
  result.m_id = touch.m_id + 1;
  result.m_location = symmetricalLocation;

  return result;
}

void MapWidget::OnViewportChanged(ScreenBase const & screen)
{
  UpdateScaleControl();
}

void MapWidget::UpdateScaleControl()
{
  if (!m_slider || m_sliderState == SliderState::Pressed)
    return;
  m_slider->SetPosWithBlockedSignals(m_framework.GetDrawScale());
}

void MapWidget::Build()
{
  std::string_view vertexSrc;
  std::string_view fragmentSrc;
  if (m_apiOpenGLES3)
  {
#if defined(OMIM_OS_LINUX)
    vertexSrc = ":common/shaders/gles_300.vsh.glsl";
    fragmentSrc = ":common/shaders/gles_300.fsh.glsl";
#else
    vertexSrc = ":common/shaders/gl_150.vsh.glsl";
    fragmentSrc = ":common/shaders/gl_150.fsh.glsl";
#endif
  }
  else
  {
    vertexSrc = ":common/shaders/gles_200.vsh.glsl";
    fragmentSrc = ":common/shaders/gles_200.fsh.glsl";
  }

  m_program = std::make_unique<QOpenGLShaderProgram>(this);
  m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, vertexSrc.data());
  m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentSrc.data());
  m_program->link();

  m_vao = std::make_unique<QOpenGLVertexArrayObject>(this);
  m_vao->create();
  m_vao->bind();

  m_vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
  m_vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
  m_vbo->create();
  m_vbo->bind();

  QVector4D vertices[4] = {QVector4D(-1.0, 1.0, 0.0, 1.0),
                           QVector4D(1.0, 1.0, 1.0, 1.0),
                           QVector4D(-1.0, -1.0, 0.0, 0.0),
                           QVector4D(1.0, -1.0, 1.0, 0.0)};
  m_vbo->allocate(static_cast<void*>(vertices), sizeof(vertices));
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
  // 0-index of the buffer is linked to "a_position" attribute in vertex shader.
  // Introduced in https://github.com/organicmaps/organicmaps/pull/9814
  f->glEnableVertexAttribArray(0);
  f->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(QVector4D), nullptr);

  m_program->release();
  m_vao->release();
}

namespace
{
search::ReverseGeocoder::Address GetFeatureAddressInfo(Framework const & framework, FeatureType & ft)
{
  search::ReverseGeocoder const coder(framework.GetDataSource());
  search::ReverseGeocoder::Address address;
  coder.GetExactAddress(ft, address);
  return address;
}
} // namespace

void MapWidget::ShowInfoPopup(QMouseEvent * e, m2::PointD const & pt)
{
  // show feature types
  QMenu menu;
  auto const addStringFn = [&menu](std::string const & s)
  {
    if (!s.empty())
      menu.addAction(QString::fromUtf8(s.c_str()));
  };

  m_framework.ForEachFeatureAtPoint([&](FeatureType & ft)
  {
    // ID
    addStringFn(DebugPrint(ft.GetID()));

    // Types
    std::string concat;
    auto types = feature::TypesHolder(ft);
    types.SortBySpec();
    for (auto const & type : types.ToObjectNames())
      concat += type + " ";
    addStringFn(concat);

    // Name
    addStringFn(std::string(ft.GetReadableName()));

    // Address
    auto const info = GetFeatureAddressInfo(m_framework, ft);
    addStringFn(info.FormatAddress());

    if (ft.GetGeomType() == feature::GeomType::Line)
    {
      // Maxspeed
      auto const & dataSource = m_framework.GetDataSource();
      auto const handle = dataSource.GetMwmHandleById(ft.GetID().m_mwmId);
      auto const speeds = routing::LoadMaxspeeds(handle);
      if (speeds)
      {
        auto const speed = speeds->GetMaxspeed(ft.GetID().m_index);
        if (speed.IsValid())
          addStringFn(DebugPrint(speed));
      }
    }

    int const layer = ft.GetLayer();
    if (layer != feature::LAYER_EMPTY)
      addStringFn("Layer = " + std::to_string(layer));

    menu.addSeparator();
  }, m_framework.PtoG(pt));

  menu.exec(e->pos());
}

void MapWidget::initializeGL()
{
  ASSERT(m_contextFactory == nullptr, ());
  if (!m_screenshotMode)
    m_ratio = devicePixelRatio();

  m_apiOpenGLES3 = true;

#if defined(OMIM_OS_LINUX)
  {
    QOpenGLFunctions * funcs = context()->functions();
    LOG(LINFO, ("Vendor:", funcs->glGetString(GL_VENDOR),
                "\nRenderer:", funcs->glGetString(GL_RENDERER),
                "\nVersion:", funcs->glGetString(GL_VERSION),
                "\nShading language version:\n",funcs->glGetString(GL_SHADING_LANGUAGE_VERSION),
                "\nExtensions:", funcs->glGetString(GL_EXTENSIONS)));

    if (!context()->isOpenGLES())
    {
      LOG(LCRITICAL, ("Context is not LibGLES! This shouldn't have happened."));
    }

    auto fmt = context()->format();
    if (context()->format().version() < qMakePair(3, 0))
    {
      LOG(LINFO, ("OpenGL ES version is below 3.0, taking the OpenGL ES 2.0 path"));
      m_apiOpenGLES3 = false;

      constexpr const char* requiredExtensions[3] =
        { "GL_EXT_map_buffer_range", "GL_OES_mapbuffer", "GL_OES_vertex_array_object" };
      for (auto & requiredExtension : requiredExtensions)
      {
        if (context()->hasExtension(QByteArray::fromStdString(requiredExtension)))
          LOG(LDEBUG, ("Found OpenGL ES 2.0 extension: ", requiredExtension));
        else
          LOG(LCRITICAL, ("A required OpenGL ES 2.0 extension is missing:", requiredExtension));
      }
      fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
      fmt.setVersion(2, 0);
    }
    else
    {
      LOG(LINFO, ("OpenGL version is at least 3.0, enabling GLSL '#version 300 es'"));
      m_apiOpenGLES3 = true;
      fmt.setVersion(3, 0);
    }


    QSurfaceFormat::setDefaultFormat(fmt);
  }
#endif

  m_contextFactory.reset(new QtOGLContextFactory(context()));

  emit BeforeEngineCreation();
  CreateEngine();
  m_framework.EnterForeground();
}

void MapWidget::paintGL()
{
  if (m_program == nullptr)
    Build();

  if (m_contextFactory->AcquireFrame())
  {
    m_vao->bind();
    m_program->bind();

    QOpenGLFunctions * funcs = context()->functions();
    funcs->glActiveTexture(GL_TEXTURE0);
    GLuint const image = m_contextFactory->GetTextureHandle();
    funcs->glBindTexture(GL_TEXTURE_2D, image);

    int const samplerLocation = m_program->uniformLocation("u_sampler");
    m_program->setUniformValue(samplerLocation, 0);

    QRectF const & texRect = m_contextFactory->GetTexRect();
    QVector2D const samplerSize(texRect.width(), texRect.height());

    int const samplerSizeLocation = m_program->uniformLocation("u_samplerSize");
    m_program->setUniformValue(samplerSizeLocation, samplerSize);

    funcs->glClearColor(0.0, 0.0, 0.0, 1.0);
    funcs->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    funcs->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_program->release();
    m_vao->release();

  }
}

void MapWidget::resizeGL(int width, int height)
{
  int const w = m_screenshotMode ? width : m_ratio * width;
  int const h = m_screenshotMode ? height : m_ratio * height;
  m_framework.OnSize(w, h);

  if (m_skin)
  {
    m_skin->Resize(w, h);

    gui::TWidgetsLayoutInfo layout;
    m_skin->ForEach(
        [&layout](gui::EWidget w, gui::Position const & pos) { layout[w] = pos.m_pixelPivot; });

    m_framework.SetWidgetLayout(std::move(layout));
  }
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mouseDoubleClickEvent(e);
  if (IsLeftButton(e))
    m_framework.Scale(Framework::SCALE_MAG_LIGHT, GetDevicePoint(e), true);
}

void MapWidget::mousePressEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mousePressEvent(e);
  if (IsLeftButton(e))
    m_framework.TouchEvent(GetDfTouchEventFromQMouseEvent(e, df::TouchEvent::TOUCH_DOWN));
}

void MapWidget::mouseMoveEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mouseMoveEvent(e);
  if (IsLeftButton(e))
    m_framework.TouchEvent(GetDfTouchEventFromQMouseEvent(e, df::TouchEvent::TOUCH_MOVE));
}

void MapWidget::mouseReleaseEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  if (e->button() == Qt::RightButton)
    emit OnContextMenuRequested(e->globalPosition().toPoint());

  QOpenGLWidget::mouseReleaseEvent(e);
  if (IsLeftButton(e))
    m_framework.TouchEvent(GetDfTouchEventFromQMouseEvent(e, df::TouchEvent::TOUCH_UP));
}

void MapWidget::wheelEvent(QWheelEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::wheelEvent(e);

  QPointF const pos = e->position();

  double const factor = e->angleDelta().y() / 3.0 / 360.0;
  // https://doc-snapshots.qt.io/qt6-dev/qwheelevent.html#angleDelta, angleDelta() returns in eighths of a degree.
  /// @todo Here you can tune the speed of zooming.
  m_framework.Scale(exp(factor), m2::PointD(L2D(pos.x()), L2D(pos.y())), false);
}
} // namespace qt::common
