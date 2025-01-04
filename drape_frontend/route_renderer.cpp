#include "drape_frontend/route_renderer.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/drape_routine.hpp"
#include "drape/vertex_array_buffer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace df
{
std::string const kRouteColor = "Route";
std::string const kRouteOutlineColor = "RouteOutline";
std::string const kRoutePedestrian = "RoutePedestrian";
std::string const kRouteBicycle = "RouteBicycle";
std::string const kRouteRuler = "RouteRuler";
std::string const kRoutePreview = "RoutePreview";
std::string const kRouteMaskCar = "RouteMaskCar";
std::string const kRouteFirstSegmentArrowsMaskCar = "RouteFirstSegmentArrowsMaskCar";
std::string const kRouteArrowsMaskCar = "RouteArrowsMaskCar";
std::string const kRouteMaskBicycle = "RouteMaskBicycle";
std::string const kRouteFirstSegmentArrowsMaskBicycle = "RouteFirstSegmentArrowsMaskBicycle";
std::string const kRouteArrowsMaskBicycle = "RouteArrowsMaskBicycle";
std::string const kRouteMaskPedestrian = "RouteMaskPedestrian";
std::string const kTransitStopInnerMarkerColor = "TransitStopInnerMarker";
std::string const kRouteFakeColor = "RouteFake";
std::string const kRouteFakeOutlineColor = "RouteFakeOutline";

namespace
{
std::array<float, 20> const kPreviewPointRadiusInPixel =
{
  // 1   2     3     4     5     6     7     8     9     10
  0.8f, 0.8f, 2.0f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f,
  //11   12    13    14    15    16    17    18    19     20
  2.5f, 2.5f, 2.5f, 2.5f, 3.0f, 4.0f, 4.5f, 4.5f, 5.0f, 5.5f
};

int const kArrowAppearingZoomLevel = 14;
int const kInvalidGroup = -1;

uint32_t const kPreviewPointsCount = 512;

double const kInvalidDistance = -1.0;

void InterpolateByZoom(SubrouteConstPtr const & subroute, ScreenBase const & screen,
                       float & halfWidth, double & zoom)
{
  int index = 0;
  float lerpCoef = 0.0f;
  ExtractZoomFactors(screen, zoom, index, lerpCoef);

  std::array<float, 20> const * halfWidthInPixel = &kRouteHalfWidthInPixelOthers;
  if (subroute->m_routeType == RouteType::Car || subroute->m_routeType == RouteType::Taxi)
    halfWidthInPixel = &kRouteHalfWidthInPixelCar;
  else if (subroute->m_routeType == RouteType::Transit)
    halfWidthInPixel = &kRouteHalfWidthInPixelTransit;

  halfWidth = InterpolateByZoomLevels(index, lerpCoef, *halfWidthInPixel);
  halfWidth *= static_cast<float>(df::VisualParams::Instance().GetVisualScale());
}

void ClipBorders(std::vector<ArrowBorders> & borders)
{
  auto invalidBorders = [](ArrowBorders const & arrowBorders)
  {
    return arrowBorders.m_groupIndex == kInvalidGroup;
  };
  borders.erase(std::remove_if(borders.begin(), borders.end(), invalidBorders), borders.end());
}

void MergeAndClipBorders(std::vector<ArrowBorders> & borders)
{
  // Initial clipping.
  ClipBorders(borders);

  if (borders.empty())
    return;

  // Mark groups.
  for (size_t i = 0; i + 1 < borders.size(); i++)
  {
    if (borders[i].m_endDistance >= borders[i + 1].m_startDistance)
      borders[i + 1].m_groupIndex = borders[i].m_groupIndex;
  }

  // Merge groups.
  int lastGroup = borders.front().m_groupIndex;
  size_t lastGroupIndex = 0;
  for (size_t i = 1; i < borders.size(); i++)
  {
    if (borders[i].m_groupIndex != lastGroup)
    {
      borders[lastGroupIndex].m_endDistance = borders[i - 1].m_endDistance;
      lastGroupIndex = i;
      lastGroup = borders[i].m_groupIndex;
    }
    else
    {
      borders[i].m_groupIndex = kInvalidGroup;
    }
  }
  borders[lastGroupIndex].m_endDistance = borders.back().m_endDistance;

  // Clip groups.
  ClipBorders(borders);
}

bool AreEqualArrowBorders(std::vector<ArrowBorders> const & borders1,
                          std::vector<ArrowBorders> const & borders2)
{
  if (borders1.size() != borders2.size())
    return false;

  for (size_t i = 0; i < borders1.size(); i++)
  {
    if (borders1[i].m_groupIndex != borders2[i].m_groupIndex)
      return false;
  }

  double const kDistanceEps = 1e-5;
  for (size_t i = 0; i < borders1.size(); i++)
  {
    if (fabs(borders1[i].m_startDistance - borders2[i].m_startDistance) > kDistanceEps)
      return false;

    if (fabs(borders1[i].m_endDistance - borders2[i].m_endDistance) > kDistanceEps)
      return false;
  }

  return true;
}

std::vector<ArrowBorders> CalculateArrowBorders(m2::RectD screenRect, double screenScale,
                                                float currentHalfWidth, SubrouteConstPtr const & subroute,
                                                double subrouteLength, double distanceFromBegin)
{
  auto const & turns = subroute->m_turns;
  if (turns.empty())
    return {};

  // Calculate arrow mercator length.
  double glbHalfLen = 0.5 * kArrowSize;
  double const glbHalfTextureWidth = currentHalfWidth * kArrowHeightFactor * screenScale;
  double const glbHalfTextureLen = glbHalfTextureWidth * kArrowAspect;
  if (glbHalfLen < glbHalfTextureLen)
    glbHalfLen = glbHalfTextureLen;

  double const glbArrowHead = 2.0 * kArrowHeadSize * glbHalfTextureLen;
  double const glbArrowTail = 2.0 * kArrowTailSize * glbHalfTextureLen;
  double const glbMinArrowSize = glbArrowHead + glbArrowTail;

  double constexpr kExtentCoef = 1.1;
  screenRect.Scale(kExtentCoef);

  // Calculate arrow borders.
  size_t constexpr kAverageArrowsCount = 10;
  std::vector<ArrowBorders> newArrowBorders;
  newArrowBorders.reserve(kAverageArrowsCount);
  auto const & polyline = subroute->m_polyline;
  for (size_t i = 0; i < turns.size(); i++)
  {
    ArrowBorders arrowBorders;
    arrowBorders.m_groupIndex = static_cast<int>(i);
    arrowBorders.m_startDistance = std::max(0.0, turns[i] - glbHalfLen * 0.8);
    arrowBorders.m_endDistance = std::min(subrouteLength, turns[i] + glbHalfLen * 1.2);

    if ((arrowBorders.m_endDistance - arrowBorders.m_startDistance) < glbMinArrowSize ||
        arrowBorders.m_startDistance < distanceFromBegin)
    {
      continue;
    }

    m2::PointD pt = polyline.GetPointByDistance(arrowBorders.m_startDistance);
    if (screenRect.IsPointInside(pt))
    {
      newArrowBorders.push_back(arrowBorders);
      continue;
    }

    pt = polyline.GetPointByDistance(arrowBorders.m_endDistance);
    if (screenRect.IsPointInside(pt))
    {
      newArrowBorders.push_back(arrowBorders);
      continue;
    }
  }

  // Merge intersected borders and clip them.
  MergeAndClipBorders(newArrowBorders);

  // Process head and tail.
  for (ArrowBorders & borders : newArrowBorders)
  {
    borders.m_startDistance += glbArrowTail;
    borders.m_endDistance -= glbArrowHead;
  }

  return newArrowBorders;
}

void BuildBuckets(ref_ptr<dp::GraphicsContext> context, RouteRenderProperty const & renderProperty,
                  ref_ptr<gpu::ProgramManager> mng)
{
  for (auto const & bucket : renderProperty.m_buckets)
    bucket->GetBuffer()->Build(context, mng->GetProgram(renderProperty.m_state.GetProgram<gpu::Program>()));
}

RouteRenderer::Subroutes::iterator FindSubroute(RouteRenderer::Subroutes & subroutes,
                                                dp::DrapeID subrouteId)
{
  return std::find_if(subroutes.begin(), subroutes.end(),
                      [&subrouteId](RouteRenderer::SubrouteInfo const & info)
  {
    return info.m_subrouteId == subrouteId;
  });
}

float GetCurrentHalfWidth(df::RouteRenderer::SubrouteInfo const & subrouteInfo)
{
  if (subrouteInfo.m_subroute->m_maxPixelWidth < 0.0f)
    return subrouteInfo.m_baseHalfWidth;

  return std::min(subrouteInfo.m_baseHalfWidth, subrouteInfo.m_subroute->m_maxPixelWidth * 0.5f);
}
}  // namespace

RouteRenderer::RouteRenderer(PreviewPointsRequestCallback && previewPointsRequest)
  : m_distanceFromBegin(kInvalidDistance)
  , m_followingEnabled(false)
  , m_previewPointsRequest(std::move(previewPointsRequest))
  , m_waitForPreviewRenderData(false)
{
  ASSERT(m_previewPointsRequest != nullptr, ());
}

void RouteRenderer::PrepareRouteArrows(ScreenBase const & screen,
                                       PrepareRouteArrowsCallback const & prepareCallback)
{
  for (auto & subrouteInfo : m_subroutes)
  {
    // Interpolate values by zoom level.
    double zoom = 0.0;
    float halfWidth = 0.0;
    InterpolateByZoom(subrouteInfo.m_subroute, screen, halfWidth, zoom);
    subrouteInfo.m_baseHalfWidth = halfWidth;

    if (zoom < kArrowAppearingZoomLevel)
    {
      subrouteInfo.m_arrowsData.reset();
      subrouteInfo.m_arrowBorders.clear();
      continue;
    }

    // Calculate arrow borders.
    double dist = kInvalidDistance;
    if (m_followingEnabled)
      dist = m_distanceFromBegin - subrouteInfo.m_subroute->m_baseDistance;

    // We run asynchronous task to calculate new positions of route arrows.
    auto const subrouteId = subrouteInfo.m_subrouteId;
    auto const screenRect = screen.ClipRect();
    auto const screenScale = screen.GetScale();
    auto const subrouteLength = subrouteInfo.m_length;
    auto subroute = subrouteInfo.m_subroute;
    dp::DrapeRoutine::RunSequential([subrouteId, screenRect, screenScale, halfWidth,
                                     subroute = std::move(subroute), subrouteLength,
                                     dist, prepareCallback]()
    {
      ASSERT(prepareCallback != nullptr, ());
      prepareCallback(subrouteId, CalculateArrowBorders(screenRect, screenScale, halfWidth,
                                                        subroute, subrouteLength, dist));
    });
  }
}

void RouteRenderer::CacheRouteArrows(ScreenBase const & screen, dp::DrapeID subrouteId,
                                     std::vector<ArrowBorders> && arrowBorders,
                                     CacheRouteArrowsCallback const & cacheCallback)
{
  ASSERT(cacheCallback != nullptr, ());
  auto const it = FindSubroute(m_subroutes, subrouteId);
  if (it == m_subroutes.end())
    return;

  auto & subrouteInfo = *it;

  double zoom = 0.0;
  float halfWidth = 0.0;
  InterpolateByZoom(subrouteInfo.m_subroute, screen, halfWidth, zoom);

  if (arrowBorders.empty() || zoom < kArrowAppearingZoomLevel)
  {
    // Clear arrows.
    subrouteInfo.m_arrowsData.reset();
    subrouteInfo.m_arrowBorders.clear();
  }
  else if (!AreEqualArrowBorders(arrowBorders, subrouteInfo.m_arrowBorders))
  {
    subrouteInfo.m_arrowBorders = std::move(arrowBorders);
    cacheCallback(subrouteInfo.m_subrouteId, subrouteInfo.m_arrowBorders);
  }
}

void RouteRenderer::UpdatePreview(ScreenBase const & screen)
{
  // Check if there are preview render data.
  if (m_previewRenderData.empty() && !m_waitForPreviewRenderData)
  {
    m_previewPointsRequest(kPreviewPointsCount);
    m_waitForPreviewRenderData = true;
  }
  if (m_waitForPreviewRenderData)
    return;

  float previewCircleRadius = 0.0;
  if (!m_previewSegments.empty())
  {
    ClearPreviewHandles();
    m_previewPivot = screen.GlobalRect().Center();
    previewCircleRadius = CalculateRadius(screen, kPreviewPointRadiusInPixel);
  }
  double const currentScaleGtoP = 1.0 / screen.GetScale();
  double const radiusMercator = previewCircleRadius / currentScaleGtoP;
  double const diameterMercator = 2.0 * radiusMercator;
  double const gapMercator = diameterMercator;
  dp::Color const circleColor = df::GetColorConstant(kRoutePreview);

  for (auto const & previewSegment : m_previewSegments)
  {
    auto const & info = previewSegment.second;
    m2::PolylineD polyline = {info.m_startPoint, info.m_finishPoint};
    double const segmentLen = polyline.GetLength();
    auto circlesCount = static_cast<size_t>(segmentLen / (diameterMercator + gapMercator));
    if (circlesCount == 0)
      circlesCount = 1;
    double const distDelta = segmentLen / circlesCount;
    for (double d = distDelta * 0.5; d < segmentLen; d += distDelta)
    {
      m2::PointD const pt = polyline.GetPointByDistance(d);
      m2::RectD const circleRect(pt.x - radiusMercator, pt.y - radiusMercator,
                                 pt.x + radiusMercator, pt.y + radiusMercator);
      if (!screen.ClipRect().IsIntersect(circleRect))
        continue;

      size_t pointIndex;
      CirclesPackHandle * h = GetPreviewHandle(pointIndex);
      if (h == nullptr)
      {
        // There is no any available handle.
        m_previewPointsRequest(kPreviewPointsCount);
        m_waitForPreviewRenderData = true;
        return;
      }

      m2::PointD const convertedPt = MapShape::ConvertToLocal(pt, m_previewPivot, kShapeCoordScalar);
      h->SetPoint(pointIndex, convertedPt, previewCircleRadius, circleColor);
    }
  }
}

void RouteRenderer::ClearPreviewHandles()
{
  for (auto & handle : m_previewHandlesCache)
  {
    handle.first->Clear();
    handle.second = 0;
  }
}

CirclesPackHandle * RouteRenderer::GetPreviewHandle(size_t & index)
{
  for (auto & handle : m_previewHandlesCache)
  {
    if (handle.second < handle.first->GetPointsCount())
    {
      index = handle.second++;
      return handle.first;
    }
  }
  index = 0;
  return nullptr;
}

// Route mask is used to adjust line color for subroutes after the first stop. So subroute to the first stop
// point has default color, while subsequent subroutes color is adjusted with RouteArrowsMaskCar,
// RouteArrowsMaskBicycle and RouteMaskPedestrian properties.
dp::Color RouteRenderer::GetRouteMaskColor(RouteType routeType, double baseDistance) const
{
  if (baseDistance != 0.0 && m_distanceFromBegin < baseDistance)
  {
    if (routeType == RouteType::Car)
      return GetColorConstant(kRouteMaskCar);
    if (routeType == RouteType::Bicycle)
      return GetColorConstant(kRouteMaskBicycle);
    if (routeType == RouteType::Pedestrian)
      return GetColorConstant(kRouteMaskPedestrian);
  }
  return dp::Color::Transparent();
}

// Arrow mask is used to adjust route arrow color. First subroute arrows color is defined
// by RouteFirstSegmentArrowsMaskCar and RouteFirstSegmentArrowsMaskBicycle properties.
// All following subroutes have arrow color defined by RouteArrowsMaskCar, RouteArrowsMaskBicycle.
dp::Color RouteRenderer::GetArrowMaskColor(RouteType routeType, double baseDistance) const
{
  if (baseDistance == 0.0)
  {
    if (routeType == RouteType::Car)
      return GetColorConstant(kRouteFirstSegmentArrowsMaskCar);
    if (routeType == RouteType::Bicycle)
      return GetColorConstant(kRouteFirstSegmentArrowsMaskBicycle);
  }
  else if (m_distanceFromBegin < baseDistance)
  {
    if (routeType == RouteType::Car)
      return GetColorConstant(kRouteArrowsMaskCar);
    if (routeType == RouteType::Bicycle)
      return GetColorConstant(kRouteArrowsMaskBicycle);
  }
  return dp::Color::Transparent();
}

void RouteRenderer::RenderSubroute(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                                   SubrouteInfo const & subrouteInfo, size_t subrouteDataIndex,
                                   ScreenBase const & screen, bool trafficShown, FrameValues const & frameValues)
{
  ASSERT_LESS(subrouteDataIndex, subrouteInfo.m_subrouteData.size(), ());
  if (subrouteInfo.m_subrouteData[subrouteDataIndex]->m_renderProperty.m_buckets.empty())
    return;

  // Skip rendering of hidden subroutes.
  if (m_hiddenSubroutes.find(subrouteInfo.m_subrouteId) != m_hiddenSubroutes.end())
    return;

  auto const & subrouteData = subrouteInfo.m_subrouteData[subrouteDataIndex];
  float const currentHalfWidth = GetCurrentHalfWidth(subrouteInfo);

  auto const screenHalfWidth = static_cast<float>(currentHalfWidth * screen.GetScale());
  auto dist = static_cast<float>(kInvalidDistance);
  if (m_followingEnabled)
  {
    auto const distanceOffset = subrouteInfo.m_subroute->m_baseDistance + subrouteData->m_distanceOffset;
    dist = static_cast<float>(m_distanceFromBegin - distanceOffset);
  }

  dp::RenderState const & state = subrouteData->m_renderProperty.m_state;
  size_t const styleIndex = subrouteData->m_styleIndex;
  ASSERT_LESS(styleIndex, subrouteInfo.m_subroute->m_style.size(), ());
  auto const & style = subrouteInfo.m_subroute->m_style[styleIndex];

  // Set up parameters.
  gpu::RouteProgramParams params;
  frameValues.SetTo(params);
  math::Matrix<float, 4, 4> mv = screen.GetModelView(subrouteData->m_pivot, kShapeCoordScalar);
  params.m_modelView = glsl::make_mat4(mv.m_data);
  params.m_color = glsl::ToVec4(df::GetColorConstant(style.m_color));
  params.m_routeParams = glsl::vec4(currentHalfWidth, screenHalfWidth, dist, trafficShown ? 1.0f : 0.0f);

  // Adjust line color depending on route type and subroute distance. After the first stop point
  // route color is adjusted according to RouteMaskCar, RouteMaskBicycle or RouteMaskPedestrian properties.
  params.m_maskColor = glsl::ToVec4(GetRouteMaskColor(subrouteData->m_subroute->m_routeType,
                                                      subrouteData->m_subroute->m_baseDistance));
  if (style.m_pattern.m_isDashed)
  {
    params.m_pattern = glsl::vec2(static_cast<float>(screenHalfWidth * style.m_pattern.m_dashLength),
                                  static_cast<float>(screenHalfWidth * style.m_pattern.m_gapLength));
  }
  else
  {
    params.m_outlineColor = glsl::ToVec4(df::GetColorConstant(style.m_outlineColor));
  }
  params.m_fakeBorders = glsl::vec2(subrouteData->m_subroute->m_headFakeDistance,
                                    subrouteData->m_subroute->m_tailFakeDistance);
  params.m_fakeColor = glsl::ToVec4(df::GetColorConstant(kRouteFakeColor));
  params.m_fakeOutlineColor = glsl::ToVec4(df::GetColorConstant(kRouteFakeOutlineColor));

  auto routeType = subrouteData->m_subroute->m_routeType;
  ref_ptr<dp::GpuProgram> prg = mng->GetProgram(
    (routeType == df::RouteType::Pedestrian || routeType == df::RouteType::Bicycle)? gpu::Program::RouteDifficulty:
        style.m_pattern.m_isDashed ? gpu::Program::RouteDash : gpu::Program::Route);
  prg->Bind();
  dp::ApplyState(context, prg, state);
  mng->GetParamsSetter()->Apply(context, prg, params);

  // Render buckets.
  auto const & clipRect = screen.ClipRect();
  CHECK_EQUAL(subrouteData->m_renderProperty.m_buckets.size(),
              subrouteData->m_renderProperty.m_boundingBoxes.size(), ());
  for (size_t i = 0; i < subrouteData->m_renderProperty.m_buckets.size(); ++i)
  {
    if (subrouteData->m_renderProperty.m_boundingBoxes[i].IsIntersect(clipRect))
      subrouteData->m_renderProperty.m_buckets[i]->Render(context, state.GetDrawAsLine());
  }
}

void RouteRenderer::RenderSubrouteArrows(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                                         SubrouteInfo const & subrouteInfo, ScreenBase const & screen,
                                         FrameValues const & frameValues)
{
  if (subrouteInfo.m_arrowsData == nullptr ||
      subrouteInfo.m_arrowsData->m_renderProperty.m_buckets.empty() ||
      m_hiddenSubroutes.find(subrouteInfo.m_subrouteId) != m_hiddenSubroutes.end())
  {
    return;
  }

  dp::RenderState const & state = subrouteInfo.m_arrowsData->m_renderProperty.m_state;
  float const currentHalfWidth = GetCurrentHalfWidth(subrouteInfo);

  // Set up parameters.
  gpu::RouteProgramParams params;
  frameValues.SetTo(params);
  math::Matrix<float, 4, 4> mv = screen.GetModelView(subrouteInfo.m_arrowsData->m_pivot,
                                                     kShapeCoordScalar);
  params.m_modelView = glsl::make_mat4(mv.m_data);
  auto const arrowHalfWidth = static_cast<float>(currentHalfWidth * kArrowHeightFactor);
  params.m_arrowHalfWidth = arrowHalfWidth;

  // Adjust arrow color depending on route type and subroute distance
  params.m_maskColor = glsl::ToVec4(GetArrowMaskColor(subrouteInfo.m_subroute->m_routeType,
                                                      subrouteInfo.m_subroute->m_baseDistance));

  ref_ptr<dp::GpuProgram> prg = mng->GetProgram(gpu::Program::RouteArrow);
  prg->Bind();
  dp::ApplyState(context, prg, state);
  mng->GetParamsSetter()->Apply(context, prg, params);

  auto const & clipRect = screen.ClipRect();
  CHECK_EQUAL(subrouteInfo.m_arrowsData->m_renderProperty.m_buckets.size(),
              subrouteInfo.m_arrowsData->m_renderProperty.m_boundingBoxes.size(), ());
  for (size_t i = 0; i < subrouteInfo.m_arrowsData->m_renderProperty.m_buckets.size(); ++i)
  {
    if (subrouteInfo.m_arrowsData->m_renderProperty.m_boundingBoxes[i].IsIntersect(clipRect))
      subrouteInfo.m_arrowsData->m_renderProperty.m_buckets[i]->Render(context, state.GetDrawAsLine());
  }
}

void RouteRenderer::RenderSubrouteMarkers(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                                          SubrouteInfo const & subrouteInfo, ScreenBase const & screen,
                                          FrameValues const & frameValues)
{
  if (subrouteInfo.m_markersData == nullptr ||
      subrouteInfo.m_markersData->m_renderProperty.m_buckets.empty() ||
      m_hiddenSubroutes.find(subrouteInfo.m_subrouteId) != m_hiddenSubroutes.end())
  {
    return;
  }

  auto dist = static_cast<float>(kInvalidDistance);
  if (m_followingEnabled)
    dist = static_cast<float>(m_distanceFromBegin - subrouteInfo.m_subroute->m_baseDistance);

  dp::RenderState const & state = subrouteInfo.m_markersData->m_renderProperty.m_state;
  float const currentHalfWidth = GetCurrentHalfWidth(subrouteInfo);

  // Set up parameters.
  gpu::RouteProgramParams params;
  frameValues.SetTo(params);
  math::Matrix<float, 4, 4> mv = screen.GetModelView(subrouteInfo.m_markersData->m_pivot,
                                                     kShapeCoordScalar);
  params.m_modelView = glsl::make_mat4(mv.m_data);
  params.m_routeParams = glsl::vec4(currentHalfWidth, dist, 0.0f, 0.0f);
  params.m_angleCosSin = glsl::vec2(static_cast<float>(cos(screen.GetAngle())),
                                    static_cast<float>(sin(screen.GetAngle())));

  // Adjust color depending on route type and subroute distance. After the first stop point
  // marker color is adjusted according to RouteMaskCar, RouteMaskBicycle or RouteMaskPedestrian properties.
  params.m_maskColor = glsl::ToVec4(GetRouteMaskColor(subrouteInfo.m_subroute->m_routeType,
                                                      subrouteInfo.m_subroute->m_baseDistance));

  ref_ptr<dp::GpuProgram> prg = mng->GetProgram(gpu::Program::RouteMarker);
  prg->Bind();
  dp::ApplyState(context, prg, state);
  mng->GetParamsSetter()->Apply(context, prg, params);
  for (auto const & bucket : subrouteInfo.m_markersData->m_renderProperty.m_buckets)
    bucket->Render(context, state.GetDrawAsLine());
}

void RouteRenderer::RenderPreviewData(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                                      ScreenBase const & screen, FrameValues const & frameValues)
{
  if (m_waitForPreviewRenderData || m_previewSegments.empty() || m_previewRenderData.empty())
    return;

  gpu::MapProgramParams params;
  frameValues.SetTo(params);
  math::Matrix<float, 4, 4> mv = screen.GetModelView(m_previewPivot, kShapeCoordScalar);
  params.m_modelView = glsl::make_mat4(mv.m_data);
  ref_ptr<dp::GpuProgram> program = mng->GetProgram(gpu::Program::CirclePoint);
  program->Bind();

  dp::RenderState const & state = m_previewRenderData.front()->m_state;
  dp::ApplyState(context, program, state);
  mng->GetParamsSetter()->Apply(context, program, params);

  ASSERT_EQUAL(m_previewRenderData.size(), m_previewHandlesCache.size(), ());
  for (size_t i = 0; i < m_previewRenderData.size(); i++)
  {
    if (m_previewHandlesCache[i].second != 0)
      m_previewRenderData[i]->m_bucket->Render(context, state.GetDrawAsLine());
  }
}

void RouteRenderer::RenderRoute(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                                ScreenBase const & screen, bool trafficShown, FrameValues const & frameValues)
{
  for (auto const & subroute : m_subroutes)
  {
    // Render subroutes.
    for (size_t i = 0; i < subroute.m_subrouteData.size(); ++i)
      RenderSubroute(context, mng, subroute, i, screen, trafficShown, frameValues);

    // Render markers.
    RenderSubrouteMarkers(context, mng, subroute, screen, frameValues);

    // Render arrows.
    RenderSubrouteArrows(context, mng, subroute, screen, frameValues);
  }

  // Render preview.
  RenderPreviewData(context, mng, screen, frameValues);
}

void RouteRenderer::AddSubrouteData(ref_ptr<dp::GraphicsContext> context,
                                    drape_ptr<SubrouteData> && subrouteData,
                                    ref_ptr<gpu::ProgramManager> mng)
{
  auto const it = FindSubroute(m_subroutes, subrouteData->m_subrouteId);
  if (it != m_subroutes.end())
  {
    if (!it->m_subrouteData.empty())
    {
      int const recacheId = it->m_subrouteData.front()->m_recacheId;
      if (recacheId < subrouteData->m_recacheId)
      {
        // Remove obsolete subroute data.
        it->m_subrouteData.clear();
        it->m_markersData.reset();
        it->m_arrowsData.reset();
        it->m_arrowBorders.clear();

        it->m_subroute = subrouteData->m_subroute;
        it->m_subrouteId = subrouteData->m_subrouteId;
        it->m_length = subrouteData->m_subroute->m_polyline.GetLength();
      }
      else if (recacheId > subrouteData->m_recacheId)
      {
        return;
      }
    }

    it->m_subrouteData.push_back(std::move(subrouteData));
    BuildBuckets(context, it->m_subrouteData.back()->m_renderProperty, mng);
  }
  else
  {
    // Add new subroute.
    SubrouteInfo info;
    info.m_subroute = subrouteData->m_subroute;
    info.m_subrouteId = subrouteData->m_subrouteId;
    info.m_length = subrouteData->m_subroute->m_polyline.GetLength();
    info.m_subrouteData.push_back(std::move(subrouteData));
    BuildBuckets(context, info.m_subrouteData.back()->m_renderProperty, mng);
    m_subroutes.push_back(std::move(info));

    std::sort(m_subroutes.begin(), m_subroutes.end(),
              [](SubrouteInfo const & info1, SubrouteInfo const & info2)
    {
      return info1.m_subroute->m_baseDistance > info2.m_subroute->m_baseDistance;
    });
  }
}

void RouteRenderer::AddSubrouteArrowsData(ref_ptr<dp::GraphicsContext> context,
                                          drape_ptr<SubrouteArrowsData> && routeArrowsData,
                                          ref_ptr<gpu::ProgramManager> mng)
{
  auto const it = FindSubroute(m_subroutes, routeArrowsData->m_subrouteId);
  if (it != m_subroutes.end())
  {
    it->m_arrowsData = std::move(routeArrowsData);
    BuildBuckets(context, it->m_arrowsData->m_renderProperty, mng);
  }
}

void RouteRenderer::AddSubrouteMarkersData(ref_ptr<dp::GraphicsContext> context,
                                           drape_ptr<SubrouteMarkersData> && subrouteMarkersData,
                                           ref_ptr<gpu::ProgramManager> mng)
{
  auto const it = FindSubroute(m_subroutes, subrouteMarkersData->m_subrouteId);
  if (it != m_subroutes.end())
  {
    it->m_markersData = std::move(subrouteMarkersData);
    BuildBuckets(context, it->m_markersData->m_renderProperty, mng);
  }
}

RouteRenderer::Subroutes const & RouteRenderer::GetSubroutes() const
{
  return m_subroutes;
}

void RouteRenderer::RemoveSubrouteData(dp::DrapeID subrouteId)
{
  auto const it = FindSubroute(m_subroutes, subrouteId);
  if (it != m_subroutes.end())
    m_subroutes.erase(it);
}

void RouteRenderer::AddPreviewRenderData(ref_ptr<dp::GraphicsContext> context,
                                         drape_ptr<CirclesPackRenderData> && renderData,
                                         ref_ptr<gpu::ProgramManager> mng)
{
  drape_ptr<CirclesPackRenderData> data = std::move(renderData);
  ref_ptr<dp::GpuProgram> program = mng->GetProgram(gpu::Program::CirclePoint);
  program->Bind();
  data->m_bucket->GetBuffer()->Build(context, program);
  m_previewRenderData.push_back(std::move(data));
  m_waitForPreviewRenderData = false;

  // Save handle in the cache.
  auto & bucket = m_previewRenderData.back()->m_bucket;
  ASSERT_EQUAL(bucket->GetOverlayHandlesCount(), 1, ());
  auto handle = static_cast<CirclesPackHandle *>(bucket->GetOverlayHandle(0).get());
  handle->Clear();
  m_previewHandlesCache.emplace_back(std::make_pair(handle, 0));
}

void RouteRenderer::ClearObsoleteData(int currentRecacheId)
{
  auto const functor = [&currentRecacheId](SubrouteInfo const & subrouteInfo)
  {
    return !subrouteInfo.m_subrouteData.empty() &&
           subrouteInfo.m_subrouteData.front()->m_recacheId < currentRecacheId;
  };
  m_subroutes.erase(std::remove_if(m_subroutes.begin(), m_subroutes.end(), functor),
                    m_subroutes.end());
}

void RouteRenderer::Clear()
{
  m_subroutes.clear();
  m_distanceFromBegin = kInvalidDistance;
}

void RouteRenderer::ClearContextDependentResources()
{
  // Here we clear only context-dependent part of subroute data.
  for (auto & subroute : m_subroutes)
  {
    subroute.m_subrouteData.clear();
    subroute.m_markersData.reset();
    subroute.m_arrowsData.reset();
    subroute.m_arrowBorders.clear();
  }

  m_previewRenderData.clear();
  m_previewHandlesCache.clear();
  m_waitForPreviewRenderData = false;
}

void RouteRenderer::UpdateDistanceFromBegin(double distanceFromBegin)
{
  m_distanceFromBegin = distanceFromBegin;
}

void RouteRenderer::SetFollowingEnabled(bool enabled)
{
  m_followingEnabled = enabled;
}

void RouteRenderer::AddPreviewSegment(dp::DrapeID id, PreviewInfo && info)
{
  m_previewSegments.insert(std::make_pair(id, std::move(info)));
}

void RouteRenderer::RemovePreviewSegment(dp::DrapeID id)
{
  m_previewSegments.erase(id);
}

void RouteRenderer::RemoveAllPreviewSegments()
{
  m_previewSegments.clear();
}

void RouteRenderer::SetSubrouteVisibility(dp::DrapeID id, bool isVisible)
{
  if (isVisible)
    m_hiddenSubroutes.erase(id);
  else
    m_hiddenSubroutes.insert(id);
}

bool RouteRenderer::HasTransitData() const
{
  for (auto const & subroute : m_subroutes)
    if (subroute.m_subroute->m_routeType == RouteType::Transit)
      return true;

  return false;
}

bool RouteRenderer::IsRulerRoute() const
{
  for (auto const & subroute : m_subroutes)
    if (subroute.m_subroute->m_routeType == RouteType::Ruler)
      return true;

  return false;
}

bool RouteRenderer::HasData() const
{
  return !m_subroutes.empty();
}

bool RouteRenderer::HasPreviewData() const
{
  return !m_previewSegments.empty() && !m_previewRenderData.empty();
}
}  // namespace df
