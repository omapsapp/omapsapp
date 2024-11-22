#include "geometry/circle_on_earth.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"
#include "geometry/point3d.hpp"

#include "base/assert.hpp"

namespace
{
std::vector<m3::PointD> CreateCircleOnNorth(double radiusMeters, double angleStepDegree)
{
  double const angle = radiusMeters / ms::kEarthRadiusMeters;
  double const circleRadiusMeters = ms::kEarthRadiusMeters * sin(angle);

  double const z = ms::kEarthRadiusMeters * cos(angle);

  std::vector<m3::PointD> result;
  double const stepRad = math::DegToRad(angleStepDegree);
  for (double angleRad = 0; angleRad < 2 * math::pi; angleRad += stepRad)
  {
    result.emplace_back(circleRadiusMeters * cos(angleRad),
                        circleRadiusMeters * sin(angleRad),
                        z);
  }
  return result;
}

ms::LatLon FromEarth3dToSpherical(m3::PointD const & vec)
{
  ASSERT(AlmostEqualAbs(vec.Length(), ms::kEarthRadiusMeters, 1e-5),
         (vec.Length(), ms::kEarthRadiusMeters));

  double sinLatRad = vec.z / ms::kEarthRadiusMeters;
  sinLatRad = math::Clamp(sinLatRad, -1.0, 1.0);
  double const cosLatRad = std::sqrt(1 - sinLatRad * sinLatRad);
  CHECK(-1.0 <= cosLatRad && cosLatRad <= 1.0, (cosLatRad));

  double const latRad = asin(sinLatRad);
  double sinLonRad = vec.y / ms::kEarthRadiusMeters / cosLatRad;
  sinLonRad = math::Clamp(sinLonRad, -1.0, 1.0);
  double lonRad = asin(sinLonRad);
  if (vec.y > 0 && vec.x < 0)
    lonRad = math::pi - lonRad;
  else if (vec.y < 0 && vec.x < 0)
    lonRad = -(math::pi - fabs(lonRad));

  auto const lat = math::RadToDeg(latRad);
  auto const lon = math::RadToDeg(lonRad);

  return {lat, lon};
}
}  // namespace

namespace ms
{
std::vector<m2::PointD> CreateCircleGeometryOnEarth(ms::LatLon const & center, double radiusMeters,
                                                    double angleStepDegree)
{
  auto const circleOnNorth = CreateCircleOnNorth(radiusMeters, angleStepDegree);
  std::vector<m2::PointD> result;
  for (auto const & point3d : circleOnNorth)
  {
    auto const rotateByLat = point3d.RotateAroundY(90.0 - center.m_lat);
    auto const rotateByLon = rotateByLat.RotateAroundZ(center.m_lon);

    auto const latlonRotated = FromEarth3dToSpherical(rotateByLon);
    result.emplace_back(mercator::FromLatLon(latlonRotated));
  }

  return result;
}
}  // namespace ms
