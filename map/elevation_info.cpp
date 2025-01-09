#include "map/elevation_info.hpp"

#include "base/logging.hpp"

#include "geometry/mercator.hpp"

using namespace geometry;
using namespace mercator;

ElevationInfo::ElevationInfo(std::vector<GeometryLine> const & lines)
{
  // Concatenate all segments.
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
  {
    auto const & line = lines[lineIndex];
    if (line.empty())
      continue;

    if (lineIndex > 0)
      m_segmentsDistances.emplace_back(m_points.back().m_distance);

    AddPoints(line, true /* new segment */);
  }
  /// @todo(KK) Implement difficulty calculation.
  m_difficulty = Difficulty::Unknown;
}

void ElevationInfo::AddGpsPoints(GpsPoints const & points)
{
  GeometryLine line;
  line.reserve(points.size());
  for (auto const & point : points)
    line.emplace_back(FromLatLon(point.m_latitude, point.m_longitude), point.m_altitude);
  AddPoints(line);
}

void ElevationInfo::AddPoints(GeometryLine const & line, bool isNewSegment)
{
  if (line.empty())
    return;

  double distance = m_points.empty() ? 0 : m_points.back().m_distance;
  for (size_t pointIndex = 0; pointIndex < line.size(); ++pointIndex)
  {
    auto const & point = line[pointIndex];
    auto const & currentPointAltitude = point.GetAltitude();

    if (m_points.empty())
    {
      m_minAltitude = currentPointAltitude;
      m_maxAltitude = currentPointAltitude;
      m_points.emplace_back(point, distance);
      continue;
    }

    m_minAltitude = std::min(m_minAltitude, currentPointAltitude);
    m_maxAltitude = std::max(m_maxAltitude, currentPointAltitude);

    if (isNewSegment && pointIndex == 0)
    {
      m_points.emplace_back(point, distance);
      continue;
    }

    auto const & previousPoint = m_points.back().m_point;
    auto const deltaAltitude = currentPointAltitude - previousPoint.GetAltitude();
    if (deltaAltitude > 0)
      m_ascent += deltaAltitude;
    else
      m_descent -= deltaAltitude;
    distance += mercator::DistanceOnEarth(previousPoint.GetPoint(), point.GetPoint());
    m_points.emplace_back(point, distance);
  }
}
