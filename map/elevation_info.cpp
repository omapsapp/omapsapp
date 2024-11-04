#include "map/elevation_info.hpp"

#include "geometry/mercator.hpp"

ElevationInfo::ElevationInfo(kml::MultiGeometry const & geometry)
{
  double distance = 0;

  // Concatenate all segments.
  for (size_t i = 0; i < geometry.m_lines.size(); ++i)
  {
    auto const & points = geometry.m_lines[i];
    if (points.empty())
      continue;

    double distanceToSegmentBegin = 0;
    if (i == 0)
    {
      m_minAltitude = points[i].GetAltitude();
      m_maxAltitude = m_minAltitude;
    }
    else
    {
      distanceToSegmentBegin = m_points.back().m_distance;
      m_segmentsDistances.emplace_back(distanceToSegmentBegin);
    }

    m_points.emplace_back(points[i], distanceToSegmentBegin);

    for (size_t j = 0; j < points.size(); ++j)
    {
      auto const & currentPoint = points[j];
      auto const & currentPointAltitude = currentPoint.GetAltitude();
      if (currentPointAltitude < m_minAltitude)
        m_minAltitude = currentPointAltitude;
      if (currentPointAltitude > m_maxAltitude)
        m_maxAltitude = currentPointAltitude;

      if (j == 0)
        continue;

      auto const & previousPoint = points[j - 1];
      distance += mercator::DistanceOnEarth(previousPoint.GetPoint(), currentPoint.GetPoint());
      m_points.emplace_back(currentPoint, distance);

      auto const deltaAltitude = currentPointAltitude - previousPoint.GetAltitude();
      if (deltaAltitude > 0)
        m_ascent += deltaAltitude;
      else
        m_descent -= deltaAltitude;
    }
  }
  /// @todo(KK) Implement difficulty calculation.
  m_difficulty = Difficulty::Unknown;
}
