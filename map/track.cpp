#include "map/track.hpp"

#include "map/bookmark_helpers.hpp"
#include "map/user_mark_id_storage.hpp"

#include "geometry/mercator.hpp"
#include "geometry/rect_intersect.hpp"

#include <utility>

namespace
{
double GetLengthInMeters(kml::MultiGeometry::LineT const & points, size_t pointIndex)
{
  CHECK_LESS(pointIndex, points.size(), (pointIndex, points.size()));

  double length = 0.0;
  for (size_t i = 1; i <= pointIndex; ++i)
  {
    auto const & pt1 = points[i - 1].GetPoint();
    auto const & pt2 = points[i].GetPoint();
    auto const segmentLength = mercator::DistanceOnEarth(pt1, pt2);
    length += segmentLength;
  }
  return length;
}
}  // namespace

Track::Track(kml::TrackData && data)
  : Base(data.m_id == kml::kInvalidTrackId ? UserMarkIdStorage::Instance().GetNextTrackId() : data.m_id)
  , m_data(std::move(data))
{
  m_data.m_id = GetId();
  CHECK(m_data.m_geometry.IsValid(), ());
}

void Track::CacheDataForInteraction() const
{
  m_interactionData = InteractionData();
  m_interactionData->m_lengths = GetLengthsImpl();
  m_interactionData->m_limitRect = GetLimitRectImpl();
}

std::vector<double> Track::GetLengthsImpl() const
{
  auto const & lines = GetData().m_geometry.m_lines;
  std::vector<double> lengths;
  lengths.emplace_back(0.0);
  for (size_t i = 0; i < lines.size(); ++i)
  {
    auto const & line = lines[i];
    for (size_t j = 1; j < line.size(); ++j)
    {
      auto const & pt1 = line[j - 1].GetPoint();
      auto const & pt2 = line[j].GetPoint();
      auto const segmentLength = mercator::DistanceOnEarth(pt1, pt2);
      lengths.emplace_back((j == 1 ? 0.0 : lengths.back()) + segmentLength);
    }
  }
  return lengths;
}

m2::RectD Track::GetLimitRectImpl() const
{
  m2::RectD limitRect;
  for (auto const & line : m_data.m_geometry.m_lines)
  {
    for (auto const & pt : line)
      limitRect.Add(pt.GetPoint());
  }
  return limitRect;
}

bool Track::HasAltitudes() const
{
  bool hasNonDefaultAltitude = false;

  for (auto const & line : m_data.m_geometry.m_lines)
  {
    for (auto const & pt : line)
    {
      if (pt.GetAltitude() == geometry::kInvalidAltitude)
        return false;
      if (!hasNonDefaultAltitude && pt.GetAltitude() != geometry::kDefaultAltitudeMeters)
        hasNonDefaultAltitude = true;
    }
  }

  return hasNonDefaultAltitude;
}

std::string Track::GetName() const
{
  return GetPreferredBookmarkStr(m_data.m_name);
}

void Track::SetName(std::string const & name)
{
    kml::SetDefaultStr(m_data.m_name, name);
}

std::string Track::GetDescription() const
{
  return GetPreferredBookmarkStr(m_data.m_description);
}

void Track::setData(kml::TrackData const & data)
{
  m_isDirty = true;
  m_data = data;
}

m2::RectD Track::GetLimitRect() const
{
  if (m_interactionData)
    return m_interactionData->m_limitRect;
  return GetLimitRectImpl();
}

double Track::GetLengthMeters() const
{
  if (m_interactionData)
    return m_interactionData->m_lengths.back();

  double len = 0;
  for (auto const & line : m_data.m_geometry.m_lines)
    len += GetLengthInMeters(line, line.size() - 1);
  return len;
}

double Track::GetLengthMetersImpl(kml::MultiGeometry::LineT const & line, size_t ptIdx) const
{
  if (!m_interactionData)
    CacheDataForInteraction();
  CHECK_LESS(ptIdx, m_interactionData->m_lengths.size(), ());
  return m_interactionData->m_lengths[ptIdx];
}

std::pair<m2::PointD, double> Track::GetCenterPoint() const
{
  ASSERT(m_data.m_geometry.IsValid(), ());

  auto const & line = m_data.m_geometry.m_lines[0];
  return { line[line.size() / 2].GetPoint(), GetLengthMetersImpl(line, line.size() / 2) };
}

void Track::UpdateSelectionInfo(m2::RectD const & touchRect, TrackSelectionInfo & info) const
{
  if (m_interactionData && !m_interactionData->m_limitRect.IsIntersect(touchRect))
    return;

  for (auto const & line : m_data.m_geometry.m_lines)
  {
    for (size_t i = 0; i + 1 < line.size(); ++i)
    {
      auto pt1 = line[i].GetPoint();
      auto pt2 = line[i + 1].GetPoint();
      if (!m2::Intersect(touchRect, pt1, pt2))
        continue;

      m2::ParametrizedSegment<m2::PointD> seg(pt1, pt2);
      auto const closestPoint = seg.ClosestPointTo(touchRect.Center());
      auto const squaredDist = closestPoint.SquaredLength(touchRect.Center());
      if (squaredDist >= info.m_squareDist)
        continue;

      info.m_squareDist = squaredDist;
      info.m_trackId = m_data.m_id;
      info.m_trackPoint = closestPoint;

      auto const segDistInMeters = mercator::DistanceOnEarth(line[i].GetPoint(), closestPoint);
      info.m_distFromBegM = segDistInMeters + GetLengthMetersImpl(line, i);
    }
  }
}

df::DepthLayer Track::GetDepthLayer() const
{
  return df::DepthLayer::UserLineLayer;
}

size_t Track::GetLayerCount() const
{
  return m_data.m_layers.size();
}

dp::Color Track::GetColor(size_t layerIndex) const
{
  CHECK_LESS(layerIndex, m_data.m_layers.size(), ());
  return dp::Color(m_data.m_layers[layerIndex].m_color.m_rgba);
}

void Track::SetColor(dp::Color color)
{
  m_isDirty = true;
  m_data.m_layers[0].m_color.m_rgba = color.GetRGBA();
}

float Track::GetWidth(size_t layerIndex) const
{
  CHECK_LESS(layerIndex, m_data.m_layers.size(), ());
  return static_cast<float>(m_data.m_layers[layerIndex].m_lineWidth);
}

float Track::GetDepth(size_t layerIndex) const
{
  return layerIndex * 10;
}

void Track::ForEachGeometry(GeometryFnT && fn) const
{
  for (auto const & line : m_data.m_geometry.m_lines)
  {
    std::vector<m2::PointD> points;
    points.reserve(line.size());
    for (auto const & pt : line)
      points.push_back(pt.GetPoint());

    fn(std::move(points));
  }
}

void Track::Attach(kml::MarkGroupId groupId)
{
  ASSERT_EQUAL(m_groupID, kml::kInvalidMarkGroupId, ());
  m_groupID = groupId;
}

void Track::Detach()
{
  m_groupID = kml::kInvalidMarkGroupId;
}

kml::MultiGeometry::LineT Track::GetGeometry() const
{
  kml::MultiGeometry::LineT geometry;
  for (auto const & line : m_data.m_geometry.m_lines)
  {
    for (size_t i = 0; i < line.size(); ++i)
      geometry.emplace_back(line[i]);
  }
  return geometry;
}

std::optional<ElevationInfo> Track::GetElevationInfo() const
{
  if (!HasAltitudes())
    return std::nullopt;
  if (!m_elevationInfo)
    m_elevationInfo = ElevationInfo(GetData().m_geometry);
  return m_elevationInfo;
}

double Track::GetDurationInSeconds() const
{
  double duration = 0.0;
  if (!m_data.m_geometry.HasTimestamps())
    return duration;
  for (size_t i = 0; i < m_data.m_geometry.m_timestamps.size(); ++i)
  {
    ASSERT(m_data.m_geometry.HasTimestampsFor(i), ());
    auto const & timestamps = m_data.m_geometry.m_timestamps[i];
    duration += timestamps.back() - timestamps.front();
  }
  return duration;
}
