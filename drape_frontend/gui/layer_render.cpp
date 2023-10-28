#include "drape_frontend/gui/choose_position_mark.hpp"
#include "drape_frontend/gui/compass.hpp"
#include "drape_frontend/gui/copyright_label.hpp"
#include "drape_frontend/gui/debug_label.hpp"
#include "drape_frontend/gui/drape_gui.hpp"
#include "drape_frontend/gui/gui_text.hpp"
#include "drape_frontend/gui/layer_render.hpp"
#include "drape_frontend/gui/ruler.hpp"
#include "drape_frontend/gui/ruler_helper.hpp"
#include "drape_frontend/gui/speed_limit.hpp"

#include "drape_frontend/visual_params.hpp"

#include "drape/batcher.hpp"
#include "drape/graphics_context.hpp"
#include "drape/render_bucket.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/stl_helpers.hpp"

#include <ios>
#include <functional>
#include <sstream>
#include <utility>

using namespace std::placeholders;

namespace gui
{
LayerRenderer::~LayerRenderer()
{
  DestroyRenderers();
}

void LayerRenderer::Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng)
{
  for (auto & r : m_renderers)
    r.second->Build(context, mng);
}

void LayerRenderer::Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                           bool routingActive, ScreenBase const & screen)
{
  if (HasWidget(gui::WIDGET_RULER))
  {
    DrapeGui::GetRulerHelper().ResetTextDirtyFlag();
    DrapeGui::GetRulerHelper().Update(screen);
  }

  for (auto & r : m_renderers)
  {
    if (routingActive && (r.first == gui::WIDGET_COMPASS || r.first == gui::WIDGET_RULER))
      continue;

    r.second->Render(context, mng, screen);
  }
}

void LayerRenderer::Merge(ref_ptr<LayerRenderer> other)
{
  bool activeOverlayFound = false;
  for (auto & r : other->m_renderers)
  {
    auto const it = m_renderers.find(r.first);
    if (it != m_renderers.end())
    {
      auto newActiveOverlay = r.second->FindHandle(m_activeOverlayId);
      bool const updateActive = (m_activeOverlay != nullptr && newActiveOverlay != nullptr);
      it->second = std::move(r.second);
      if (!activeOverlayFound && updateActive)
      {
        activeOverlayFound = true;
        m_activeOverlay = newActiveOverlay;
        if (m_activeOverlay != nullptr)
          m_activeOverlay->OnTapBegin();
      }
    }
    else
    {
      m_renderers.insert(std::make_pair(r.first, std::move(r.second)));
    }
  }
  other->m_renderers.clear();
}

void LayerRenderer::SetLayout(TWidgetsLayoutInfo const & info)
{
  for (auto const & node : info)
  {
    auto renderer = m_renderers.find(node.first);
    if (renderer != m_renderers.end())
      renderer->second->SetPivot(m2::PointF(node.second));
  }
}

void LayerRenderer::DestroyRenderers()
{
  m_renderers.clear();
}

void LayerRenderer::AddShapeRenderer(EWidget widget, drape_ptr<ShapeRenderer> && shape)
{
  if (shape == nullptr)
    return;

  VERIFY(m_renderers.insert(std::make_pair(widget, std::move(shape))).second, ());
}

bool LayerRenderer::OnTouchDown(m2::RectD const & touchArea)
{
  for (auto & r : m_renderers)
  {
    m_activeOverlay = r.second->ProcessTapEvent(touchArea);
    if (m_activeOverlay != nullptr)
    {
      m_activeOverlayId = m_activeOverlay->GetOverlayID().m_featureId;
      m_activeOverlay->OnTapBegin();
      return true;
    }
  }

  return false;
}

void LayerRenderer::OnTouchUp(m2::RectD const & touchArea)
{
  if (m_activeOverlay != nullptr)
  {
    if (m_activeOverlay->IsTapped(touchArea))
      m_activeOverlay->OnTap();

    m_activeOverlay->OnTapEnd();
    m_activeOverlay = nullptr;
    m_activeOverlayId = FeatureID();
  }
}

void LayerRenderer::OnTouchCancel(m2::RectD const & touchArea)
{
  UNUSED_VALUE(touchArea);
  if (m_activeOverlay != nullptr)
  {
    m_activeOverlay->OnTapEnd();
    m_activeOverlay = nullptr;
    m_activeOverlayId = FeatureID();
  }
}

bool LayerRenderer::HasWidget(EWidget widget) const
{
  return m_renderers.find(widget) != m_renderers.end();
}

namespace
{
class ScaleFpsLabelHandle : public MutableLabelHandle
{
  using TBase = MutableLabelHandle;
public:
  ScaleFpsLabelHandle(uint32_t id, ref_ptr<dp::TextureManager> textures, std::string const & apiLabel, Position const & position)
    : TBase(id, position.m_anchor, position.m_pixelPivot, textures)
    , m_apiLabel(apiLabel)
  {
    SetIsVisible(true);
  }

  bool Update(ScreenBase const & screen) override
  {
    auto & helper = gui::DrapeGui::Instance().GetScaleFpsHelper();
    if (!helper.IsVisible())
      return false;

    if (m_scale != helper.GetScale() || m_fps != helper.GetFps() || m_isPaused != helper.IsPaused())
    {
      m_scale = helper.GetScale();
      m_fps = helper.GetFps();
      m_isPaused = helper.IsPaused();
      std::stringstream ss;
      ss << m_apiLabel << ": Scale: " << m_scale << " / FPS: " << m_fps;
      if (m_isPaused)
        ss << " (PAUSED)";
      SetContent(ss.str());
    }

    return TBase::Update(screen);
  }

private:
  std::string const m_apiLabel;
  int m_scale = 1;
  uint32_t m_fps = 0;
  bool m_isPaused = false;
};
}  // namespace

drape_ptr<LayerRenderer> LayerCacher::RecacheWidgets(ref_ptr<dp::GraphicsContext> context,
                                                     TWidgetsInitInfo const & initInfo,
                                                     ref_ptr<dp::TextureManager> textures)
{
  using TCacheShape = std::function<m2::PointF(ref_ptr<dp::GraphicsContext>, Position anchor,
                                               ref_ptr<LayerRenderer> renderer,
                                               ref_ptr<dp::TextureManager> textures)>;
  static std::map<EWidget, TCacheShape> cacheFunctions{
      {WIDGET_COMPASS, std::bind(&LayerCacher::CacheCompass, this, _1, _2, _3, _4)},
      {WIDGET_RULER, std::bind(&LayerCacher::CacheRuler, this, _1, _2, _3, _4)},
      {WIDGET_COPYRIGHT, std::bind(&LayerCacher::CacheCopyright, this, _1, _2, _3, _4)},
      {WIDGET_SCALE_FPS_LABEL, std::bind(&LayerCacher::CacheScaleFpsLabel, this, _1, _2, _3, _4)},
      {WIDGET_SPEED_LIMIT, std::bind(&LayerCacher::CacheSpeedLimit, this, _1, _2, _3, _4)},
  };

  drape_ptr<LayerRenderer> renderer = make_unique_dp<LayerRenderer>();
  for (auto const & node : initInfo)
  {
    auto cacheFunction = cacheFunctions.find(node.first);
    if (cacheFunction != cacheFunctions.end())
      cacheFunction->second(context, node.second, make_ref(renderer), textures);
  }

  // Flush gui geometry.
  context->Flush();

  return renderer;
}

drape_ptr<LayerRenderer> LayerCacher::RecacheChoosePositionMark(ref_ptr<dp::GraphicsContext> context,
                                                                ref_ptr<dp::TextureManager> textures)
{
  m2::PointF const surfSize = DrapeGui::Instance().GetSurfaceSize();
  drape_ptr<LayerRenderer> renderer = make_unique_dp<LayerRenderer>();

  ChoosePositionMark positionMark = ChoosePositionMark(Position(surfSize * 0.5f, dp::Center));
  renderer->AddShapeRenderer(WIDGET_CHOOSE_POSITION_MARK, positionMark.Draw(context, textures));

  // Flush gui geometry.
  context->Flush();

  return renderer;
}

#ifdef RENDER_DEBUG_INFO_LABELS
drape_ptr<LayerRenderer> LayerCacher::RecacheDebugLabels(ref_ptr<dp::GraphicsContext> context,
                                                         ref_ptr<dp::TextureManager> textures)
{
  drape_ptr<LayerRenderer> renderer = make_unique_dp<LayerRenderer>();

  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  DebugInfoLabels debugLabels = DebugInfoLabels(Position(m2::PointF(10.0f * vs, 50.0f * vs), dp::Center));

  debugLabels.AddLabel(textures, "visible: km2, readed: km2, ratio:",
                       [](ScreenBase const & screen, string & content) -> bool
  {
    double const sizeX = screen.PixelRectIn3d().SizeX();
    double const sizeY = screen.PixelRectIn3d().SizeY();

    m2::PointD const p0 = screen.PtoG(screen.P3dtoP(m2::PointD(0.0, 0.0)));
    m2::PointD const p1 = screen.PtoG(screen.P3dtoP(m2::PointD(0.0, sizeY)));
    m2::PointD const p2 = screen.PtoG(screen.P3dtoP(m2::PointD(sizeX, sizeY)));
    m2::PointD const p3 = screen.PtoG(screen.P3dtoP(m2::PointD(sizeX, 0.0)));

    double const areaG = mercator::AreaOnEarth(p0, p1, p2) + mercator::AreaOnEarth(p2, p3, p0);

    double const sizeX_2d = screen.PixelRect().SizeX();
    double const sizeY_2d = screen.PixelRect().SizeY();

    m2::PointD const p0_2d = screen.PtoG(m2::PointD(0.0, 0.0));
    m2::PointD const p1_2d = screen.PtoG(m2::PointD(0.0, sizeY_2d));
    m2::PointD const p2_2d = screen.PtoG(m2::PointD(sizeX_2d, sizeY_2d));
    m2::PointD const p3_2d = screen.PtoG(m2::PointD(sizeX_2d, 0.0));

    double const areaGTotal = mercator::AreaOnEarth(p0_2d, p1_2d, p2_2d) +
        mercator::AreaOnEarth(p2_2d, p3_2d, p0_2d);

    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << "visible: " << areaG / 1000000.0 << " km2"
        << ", readed: " << areaGTotal / 1000000.0 << " km2"
        << ", ratio: " << areaGTotal / areaG;
    content.assign(out.str());
    return true;
  });

  debugLabels.AddLabel(textures, "scale2d: m/px, scale2d * vs: m/px",
                       [](ScreenBase const & screen, string & content) -> bool
  {
    double const distanceG = mercator::DistanceOnEarth(screen.PtoG(screen.PixelRect().LeftBottom()),
                                                       screen.PtoG(screen.PixelRect().RightBottom()));

    double const vs = df::VisualParams::Instance().GetVisualScale();
    double const scale = distanceG / screen.PixelRect().SizeX();

    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << "scale2d: " << scale << " m/px"
        << ", scale2d * vs: " << scale * vs << " m/px";
    content.assign(out.str());
    return true;
  });

  debugLabels.AddLabel(textures, "distance: m",
                       [](ScreenBase const & screen, string & content) -> bool
  {
    double const sizeX = screen.PixelRectIn3d().SizeX();
    double const sizeY = screen.PixelRectIn3d().SizeY();

    double const distance = mercator::DistanceOnEarth(
      screen.PtoG(screen.P3dtoP(m2::PointD(sizeX / 2.0, 0.0))),
      screen.PtoG(screen.P3dtoP(m2::PointD(sizeX / 2.0, sizeY))));

    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << "distance: " << distance << " m";
    content.assign(out.str());
    return true;
  });

  debugLabels.AddLabel(textures, "angle: ",
                       [](ScreenBase const & screen, string & content) -> bool
  {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << "angle: " << screen.GetRotationAngle() * 180.0 / math::pi;
    content.assign(out.str());
    return true;
  });

  renderer->AddShapeRenderer(WIDGET_DEBUG_INFO, debugLabels.Draw(context, textures));

  // Flush gui geometry.
  context->Flush();

  return renderer;
}
#endif

m2::PointF LayerCacher::CacheCompass(ref_ptr<dp::GraphicsContext> context,
                                     Position const & position, ref_ptr<LayerRenderer> renderer,
                                     ref_ptr<dp::TextureManager> textures)
{
  m2::PointF compassSize;
  Compass compass = Compass(position);
  drape_ptr<ShapeRenderer> shape = compass.Draw(context, compassSize, textures,
    std::bind(&DrapeGui::CallOnCompassTappedHandler, &DrapeGui::Instance()));

  renderer->AddShapeRenderer(WIDGET_COMPASS, std::move(shape));

  return compassSize;
}

m2::PointF LayerCacher::CacheRuler(ref_ptr<dp::GraphicsContext> context, Position const & position,
                                   ref_ptr<LayerRenderer> renderer,
                                   ref_ptr<dp::TextureManager> textures)
{
  m2::PointF rulerSize;
  renderer->AddShapeRenderer(WIDGET_RULER, Ruler(position).Draw(context, rulerSize, textures));
  return rulerSize;
}

m2::PointF LayerCacher::CacheCopyright(ref_ptr<dp::GraphicsContext> context,
                                       Position const & position, ref_ptr<LayerRenderer> renderer,
                                       ref_ptr<dp::TextureManager> textures)
{
  m2::PointF size;
  renderer->AddShapeRenderer(WIDGET_COPYRIGHT, CopyrightLabel(position).Draw(context, size, textures));
  return size;
}

m2::PointF LayerCacher::CacheScaleFpsLabel(ref_ptr<dp::GraphicsContext> context,
                                           Position const & position,
                                           ref_ptr<LayerRenderer> renderer,
                                           ref_ptr<dp::TextureManager> textures)
{
  MutableLabelDrawer::Params params;
  params.m_alphabet = "MGLFPSAUEDVcale: 1234567890/()";
  params.m_maxLength = 50;
  params.m_anchor = position.m_anchor;
  params.m_font = DrapeGui::GetGuiTextFont();
  params.m_pivot = position.m_pixelPivot;
  auto const apiVersion = context->GetApiVersion();
  params.m_handleCreator = [textures, apiVersion, &position](dp::Anchor, m2::PointF const &)
  {
    std::string apiLabel;
    switch (apiVersion)
    {
    case dp::ApiVersion::OpenGLES2: apiLabel = "GL2"; break;
    case dp::ApiVersion::OpenGLES3: apiLabel = "GL3"; break;
    case dp::ApiVersion::Metal: apiLabel = "M"; break;
    case dp::ApiVersion::Vulkan: apiLabel = "V"; break;
    case dp::ApiVersion::Invalid: CHECK(false, ("Invalid API version.")); break;
    }
    return make_unique_dp<ScaleFpsLabelHandle>(EGuiHandle::GuiHandleScaleLabel, textures, apiLabel, position);
  };

  drape_ptr<ShapeRenderer> scaleRenderer = make_unique_dp<ShapeRenderer>();
  m2::PointF size = MutableLabelDrawer::Draw(context, params, textures,
    std::bind(&ShapeRenderer::AddShape, scaleRenderer.get(), _1, _2));

  renderer->AddShapeRenderer(WIDGET_SCALE_FPS_LABEL, std::move(scaleRenderer));
  return size;
}

m2::PointF LayerCacher::CacheSpeedLimit(ref_ptr<dp::GraphicsContext> context,
                                        Position const & position, ref_ptr<LayerRenderer> renderer,
                                        ref_ptr<dp::TextureManager> textures)
{
  drape_ptr<ShapeRenderer> shape = SpeedLimit(position).Draw(context, textures);
  renderer->AddShapeRenderer(WIDGET_SPEED_LIMIT, std::move(shape));

  return {};
}
}  // namespace gui
