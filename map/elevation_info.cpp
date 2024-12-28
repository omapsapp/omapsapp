#include "map/elevation_info.hpp"

#include "base/logging.hpp"

#include "geometry/mercator.hpp"

ElevationInfo::ElevationInfo(kml::MultiGeometry const & geometry)
{
  double distance = 0;
  // Concatenate all segments.
  for (size_t lineIndex = 0; lineIndex < geometry.m_lines.size(); ++lineIndex)
  {
    auto const & line = geometry.m_lines[lineIndex];
    if (line.empty())
    {
      LOG(LWARNING, ("Empty line in elevation info"));
      continue;
    }

    if (lineIndex == 0)
    {
      m_minAltitude = line.front().GetAltitude();
      m_maxAltitude = m_minAltitude;
    }

    if (lineIndex > 0)
      m_segmentsDistances.emplace_back(distance);

    for (size_t pointIndex = 0; pointIndex < line.size(); ++pointIndex)
    {
      auto const & currentPoint = line[pointIndex];
      auto const & currentPointAltitude = currentPoint.GetAltitude();
      if (currentPointAltitude < m_minAltitude)
        m_minAltitude = currentPointAltitude;
      if (currentPointAltitude > m_maxAltitude)
        m_maxAltitude = currentPointAltitude;

      if (pointIndex == 0)
      {
        m_points.emplace_back(currentPoint, distance);
        continue;
      }

      auto const & previousPoint = line[pointIndex - 1];
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
