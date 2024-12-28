#include "drape_frontend/gps_track_renderer.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/vertex_array_buffer.hpp"

#include "indexer/map_style_reader.hpp"

#include <algorithm>
#include <array>

namespace df
{
namespace
{
df::ColorConstant const kTrackUnknownDistanceColor = "TrackUnknownDistance";
df::ColorConstant const kTrackCarSpeedColor = "TrackCarSpeed";
df::ColorConstant const kTrackPlaneSpeedColor = "TrackPlaneSpeed";
df::ColorConstant const kTrackHumanSpeedColor = "TrackHumanSpeed";

int const kMinVisibleZoomLevel = 5;

uint32_t const kAveragePointsCount = 512;

// Radius of circles depending on zoom levels.
std::array<float, 20> const kRadiusInPixel =
{
  // 1   2     3     4     5     6     7     8     9     10
  0.8f, 0.8f, 1.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f,
  //11   12    13    14    15    16    17    18    19     20
  2.5f, 2.5f, 2.5f, 2.5f, 3.0f, 4.0f, 4.5f, 4.5f, 5.0f, 5.5f
};

double const kHumanSpeed = 2.6; // meters per second
double const kCarSpeed = 6.2; // meters per second
uint8_t const kMinDayAlpha = 90;
uint8_t const kMaxDayAlpha = 144;
uint8_t const kMinNightAlpha = 50;
uint8_t const kMaxNightAlpha = 102;
double const kUnknownDistanceTime = 5 * 60; // seconds

double const kDistanceScalar = 0.4;

#ifdef DEBUG
bool GpsPointsSortPredicate(GpsTrackPoint const & pt1, GpsTrackPoint const & pt2)
{
  return pt1.m_id < pt2.m_id;
}
#endif
} // namespace

GpsTrackRenderer::GpsTrackRenderer(TRenderDataRequestFn const & dataRequestFn)
  : m_dataRequestFn(dataRequestFn)
  , m_needUpdate(false)
  , m_waitForRenderData(false)
  , m_radius(0.0f)
{
  ASSERT(m_dataRequestFn != nullptr, ());
  m_points.reserve(kAveragePointsCount);
  m_handlesCache.reserve(8);
}

void GpsTrackRenderer::AddRenderData(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<gpu::ProgramManager> mng,
                                     drape_ptr<CirclesPackRenderData> && renderData)
{
  drape_ptr<CirclesPackRenderData> data = std::move(renderData);
  ref_ptr<dp::GpuProgram> program = mng->GetProgram(gpu::Program::CirclePoint);
  program->Bind();
  data->m_bucket->GetBuffer()->Build(context, program);
  m_renderData.push_back(std::move(data));
  m_waitForRenderData = false;
}

void GpsTrackRenderer::ClearRenderData()
{
  m_renderData.clear();
  m_handlesCache.clear();
  m_waitForRenderData = false;
  m_needUpdate = true;
}

void GpsTrackRenderer::UpdatePoints(std::vector<GpsTrackPoint> const & toAdd,
                                    std::vector<uint32_t> const & toRemove)
{
  bool wasChanged = false;
  if (!toRemove.empty())
  {
    auto removePredicate = [&toRemove](GpsTrackPoint const & pt)
    {
      return base::IsExist(toRemove, pt.m_id);
    };
    m_points.erase(std::remove_if(m_points.begin(), m_points.end(), removePredicate),
                   m_points.end());
    wasChanged = true;
  }

  if (!toAdd.empty())
  {
    ASSERT(is_sorted(toAdd.begin(), toAdd.end(), GpsPointsSortPredicate), ());
    ASSERT(m_points.empty() || GpsPointsSortPredicate(m_points.back(), toAdd.front()), ());
    m_points.insert(m_points.end(), toAdd.begin(), toAdd.end());
    wasChanged = true;
  }

  if (wasChanged)
  {
    m_pointsSpline = m2::Spline(m_points.size());
    for (size_t i = 0; i < m_points.size(); i++)
      m_pointsSpline.AddPoint(m_points[i].m_point);
  }

  m_needUpdate = wasChanged;
}

size_t GpsTrackRenderer::GetAvailablePointsCount() const
{
  size_t pointsCount = 0;
  for (size_t i = 0; i < m_renderData.size(); i++)
    pointsCount += m_renderData[i]->m_pointsCount;

  return pointsCount;
}

dp::Color GpsTrackRenderer::CalculatePointColor(size_t pointIndex, m2::PointD const & curPoint,
                                                double lengthFromStart, double fullLength) const
{
  ASSERT_LESS(pointIndex, m_points.size(), ());
  if (pointIndex + 1 == m_points.size())
    return dp::Color::Transparent();

  GpsTrackPoint const & start = m_points[pointIndex];
  GpsTrackPoint const & end = m_points[pointIndex + 1];

  double startAlpha = kMinDayAlpha;
  double endAlpha = kMaxDayAlpha;
  auto const style = GetStyleReader().GetCurrentStyle();
  if (style == MapStyle::MapStyleDefaultDark)
  {
    startAlpha = kMinNightAlpha;
    endAlpha = kMaxNightAlpha;
  }

  double const ta = base::Clamp(lengthFromStart / fullLength, 0.0, 1.0);
  double const alpha = startAlpha * (1.0 - ta) + endAlpha * ta;

  if ((end.m_timestamp - start.m_timestamp) > kUnknownDistanceTime)
  {
    dp::Color const color = df::GetColorConstant(df::kTrackUnknownDistanceColor);
    return dp::Color(color.GetRed(), color.GetGreen(), color.GetBlue(),
                     static_cast<uint8_t>(alpha));
  }

  double const length = (end.m_point - start.m_point).Length();
  double const dist = (curPoint - start.m_point).Length();
  double const td = base::Clamp(dist / length, 0.0, 1.0);

  double const speed = std::max(start.m_speedMPS * (1.0 - td) + end.m_speedMPS * td, 0.0);
  dp::Color const color = GetColorBySpeed(speed);
  return dp::Color(color.GetRed(), color.GetGreen(), color.GetBlue(),
                   static_cast<uint8_t>(alpha));
}

dp::Color GpsTrackRenderer::GetColorBySpeed(double speed) const
{
  if (speed > kHumanSpeed && speed <= kCarSpeed)
    return df::GetColorConstant(df::kTrackCarSpeedColor);
  else if (speed > kCarSpeed)
    return df::GetColorConstant(df::kTrackPlaneSpeedColor);

  return df::GetColorConstant(df::kTrackHumanSpeedColor);
}

void GpsTrackRenderer::RenderTrack(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                                   ScreenBase const & screen, int zoomLevel, FrameValues const & frameValues)
{
  if (zoomLevel < kMinVisibleZoomLevel)
    return;

  if (m_needUpdate)
  {
    // Skip rendering if there is no any point.
    if (m_points.empty())
    {
      m_needUpdate = false;
      return;
    }

    // Check if there are render data.
    if (m_renderData.empty() && !m_waitForRenderData)
    {
      m_dataRequestFn(kAveragePointsCount);
      m_waitForRenderData = true;
    }

    if (m_waitForRenderData)
      return;

    m_radius = CalculateRadius(screen, kRadiusInPixel);
    double const currentScaleGtoP = 1.0 / screen.GetScale();
    double const radiusMercator = m_radius / currentScaleGtoP;
    double const diameterMercator = 2.0 * radiusMercator;
    double const step = diameterMercator + kDistanceScalar * diameterMercator;

    // Update points' positions and colors.
    ASSERT(!m_renderData.empty(), ());
    m_handlesCache.clear();
    for (size_t i = 0; i < m_renderData.size(); i++)
    {
      auto & bucket = m_renderData[i]->m_bucket;
      ASSERT_EQUAL(bucket->GetOverlayHandlesCount(), 1, ());
      CirclesPackHandle * handle = static_cast<CirclesPackHandle *>(bucket->GetOverlayHandle(0).get());
      handle->Clear();
      m_handlesCache.push_back(std::make_pair(handle, 0));
    }

    m_pivot = screen.GlobalRect().Center();

    size_t cacheIndex = 0;
    if (m_points.size() == 1)
    {
      dp::Color const color = GetColorBySpeed(m_points.front().m_speedMPS);
      m2::PointD const pt = MapShape::ConvertToLocal(m_points.front().m_point, m_pivot, kShapeCoordScalar);
      m_handlesCache[cacheIndex].first->SetPoint(0, pt, m_radius, color);
      m_handlesCache[cacheIndex].second++;
    }
    else
    {
      m2::Spline::iterator it;
      it.Attach(m_pointsSpline);
      auto const fullLength = it.GetFullLength();
      double lengthFromStart = 0.0;
      while (!it.BeginAgain())
      {
        m2::PointD const pt = it.m_pos;
        m2::RectD pointRect(pt.x - radiusMercator, pt.y - radiusMercator,
                            pt.x + radiusMercator, pt.y + radiusMercator);
        if (screen.ClipRect().IsIntersect(pointRect))
        {
          dp::Color const color = CalculatePointColor(it.GetIndex(), pt, lengthFromStart, fullLength);
          m2::PointD const convertedPt = MapShape::ConvertToLocal(pt, m_pivot, kShapeCoordScalar);
          m_handlesCache[cacheIndex].first->SetPoint(m_handlesCache[cacheIndex].second,
                                                     convertedPt, m_radius, color);
          m_handlesCache[cacheIndex].second++;
          if (m_handlesCache[cacheIndex].second >= m_handlesCache[cacheIndex].first->GetPointsCount())
            cacheIndex++;

          if (cacheIndex >= m_handlesCache.size())
          {
            m_dataRequestFn(kAveragePointsCount);
            m_waitForRenderData = true;
            return;
          }
        }
        lengthFromStart += step;
        it.Advance(step);
      }

#ifdef GPS_TRACK_SHOW_RAW_POINTS
      for (size_t i = 0; i < m_points.size(); i++)
      {
        m2::PointD const convertedPt = MapShape::ConvertToLocal(m_points[i].m_point, m_pivot, kShapeCoordScalar);
        m_handlesCache[cacheIndex].first->SetPoint(m_handlesCache[cacheIndex].second, convertedPt,
                                                   m_radius * 1.2, dp::Color(0, 0, 255, 255));
        m_handlesCache[cacheIndex].second++;
        if (m_handlesCache[cacheIndex].second >= m_handlesCache[cacheIndex].first->GetPointsCount())
          cacheIndex++;

        if (cacheIndex >= m_handlesCache.size())
        {
          m_dataRequestFn(kAveragePointsCount);
          m_waitForRenderData = true;
          return;
        }
      }
#endif
    }
    m_needUpdate = false;
  }

  if (m_handlesCache.empty() || m_handlesCache.front().second == 0)
    return;

  ASSERT_LESS_OR_EQUAL(m_renderData.size(), m_handlesCache.size(), ());

  // Render points.
  gpu::MapProgramParams params;
  frameValues.SetTo(params);
  math::Matrix<float, 4, 4> mv = screen.GetModelView(m_pivot, kShapeCoordScalar);
  params.m_modelView = glsl::make_mat4(mv.m_data);
  ref_ptr<dp::GpuProgram> program = mng->GetProgram(gpu::Program::CirclePoint);
  program->Bind();

  ASSERT_GREATER(m_renderData.size(), 0, ());
  dp::RenderState const & state = m_renderData.front()->m_state;
  dp::ApplyState(context, program, state);
  mng->GetParamsSetter()->Apply(context, program, params);

  for (size_t i = 0; i < m_renderData.size(); i++)
  {
    if (m_handlesCache[i].second != 0)
      m_renderData[i]->m_bucket->Render(context, state.GetDrawAsLine());
  }
}

void GpsTrackRenderer::Update()
{
  m_needUpdate = true;
}

void GpsTrackRenderer::Clear()
{
  m_points.clear();
  ClearRenderData();
}
}  // namespace df
