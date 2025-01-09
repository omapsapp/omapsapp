#include "map/elevation_info.hpp"

#include "base/logging.hpp"

#include "geometry/mercator.hpp"

ElevationInfo::ElevationInfo(std::vector<GeometryLine> const & lines)
{
  // Concatenate all segments.
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
  {
    auto const & line = lines[lineIndex];
    if (line.empty())
    {
      LOG(LWARNING, ("Empty line in elevation info"));
      continue;
    }

    if (lineIndex > 0)
      m_segmentsDistances.emplace_back(m_points.back().m_distance);

    AddPoints(line);
  }
  /// @todo(KK) Implement difficulty calculation.
  m_difficulty = Difficulty::Unknown;
}

void ElevationInfo::AddGpsPoints(GpsPoints const & points)
{
  GeometryLine line;
  line.reserve(points.size());
  for (auto const & point : points)
    line.emplace_back(mercator::FromLatLon(point.m_latitude, point.m_longitude), point.m_altitude);
  AddPoints(line);
}

void ElevationInfo::AddPoints(GeometryLine const & line)
{
  double distance;
  if (m_points.empty())
  {
    m_minAltitude = line[0].GetAltitude();
    m_maxAltitude = m_minAltitude;
    distance = 0;
  }
  else
    distance = m_points.back().m_distance;

  for (size_t pointIndex = 0; pointIndex < line.size(); ++pointIndex)
  {
    auto const & currentPoint = line[pointIndex];
    auto const & currentPointAltitude = currentPoint.GetAltitude();
    m_minAltitude = std::min(m_minAltitude, currentPointAltitude);
    m_maxAltitude = std::max(m_maxAltitude, currentPointAltitude);

    if (pointIndex == 0)
    {
      m_points.emplace_back(currentPoint, distance);
      continue;
    }

    auto const & previousPoint = line[pointIndex - 1];
    auto const deltaAltitude = currentPointAltitude - previousPoint.GetAltitude();
    if (deltaAltitude > 0)
      m_ascent += deltaAltitude;
    else
      m_descent -= deltaAltitude;

    distance += mercator::DistanceOnEarth(previousPoint.GetPoint(), currentPoint.GetPoint());
    m_points.emplace_back(currentPoint, distance);
  }
}

