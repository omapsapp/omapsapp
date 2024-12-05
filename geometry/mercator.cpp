#include "geometry/mercator.hpp"

#include "geometry/area_on_earth.hpp"
#include "geometry/distance_on_sphere.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <cmath>

namespace mercator
{
m2::RectD MetersToXY(double lon, double lat, double lonMetersR, double latMetersR)
{
  using std::cos, std::fabs, std::max, std::min;

  double const latDegreeOffset = latMetersR * Bounds::kDegreesInMeter;
  double const minLat = max(-90.0, lat - latDegreeOffset);
  double const maxLat = min(90.0, lat + latDegreeOffset);

  double const cosL = max(cos(math::DegToRad(max(fabs(minLat), fabs(maxLat)))), 0.00001);
  ASSERT_GREATER(cosL, 0.0, ());

  double const lonDegreeOffset = lonMetersR * Bounds::kDegreesInMeter / cosL;
  double const minLon = max(-180.0, lon - lonDegreeOffset);
  double const maxLon = min(180.0, lon + lonDegreeOffset);

  return {FromLatLon(minLat, minLon), FromLatLon(maxLat, maxLon)};
}

m2::PointD GetSmPoint(m2::PointD const & pt, double lonMetersR, double latMetersR)
{
  using std::max, std::min;

  double const lat = YToLat(pt.y);
  double const lon = XToLon(pt.x);

  double const latDegreeOffset = latMetersR * Bounds::kDegreesInMeter;
  double const newLat = min(90.0, max(-90.0, lat + latDegreeOffset));

  double const cosL = max(cos(math::DegToRad(newLat)), 0.00001);
  ASSERT_GREATER(cosL, 0.0, ());

  double const lonDegreeOffset = lonMetersR * Bounds::kDegreesInMeter / cosL;
  double const newLon = min(180.0, max(-180.0, lon + lonDegreeOffset));

  return FromLatLon(newLat, newLon);
}

double DistanceOnEarth(m2::PointD const & p1, m2::PointD const & p2)
{
  return ms::DistanceOnEarth(ToLatLon(p1), ToLatLon(p2));
}

double AreaOnEarth(m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
{
  return ms::AreaOnEarth(ToLatLon(p1), ToLatLon(p2), ToLatLon(p3));
}

double AreaOnEarth(m2::RectD const & rect)
{
  return AreaOnEarth(rect.LeftTop(), rect.LeftBottom(), rect.RightBottom()) +
         AreaOnEarth(rect.LeftTop(), rect.RightTop(), rect.RightBottom());
}

void ClampPoint(m2::PointD & pt)
{
  pt.x = ClampX(pt.x);
  pt.y = ClampY(pt.y);
}

double YToLat(double y)
{
  return math::RadToDeg(2.0 * atan(tanh(0.5 * math::DegToRad(y))));
}

double LatToY(double lat)
{
  double const sinx = sin(math::DegToRad(math::Clamp(lat, -86.0, 86.0)));
  double const res = math::RadToDeg(0.5 * log((1.0 + sinx) / (1.0 - sinx)));
  return ClampY(res);
}

m2::RectD MetersToXY(double lon, double lat, double metersR)
{
  return MetersToXY(lon, lat, metersR, metersR);
}

m2::RectD RectByCenterXYAndSizeInMeters(double centerX, double centerY, double sizeX, double sizeY)
{
  ASSERT_GREATER_OR_EQUAL(sizeX, 0, ());
  ASSERT_GREATER_OR_EQUAL(sizeY, 0, ());

  return MetersToXY(XToLon(centerX), YToLat(centerY), sizeX, sizeY);
}

m2::RectD RectByCenterXYAndSizeInMeters(m2::PointD const & center, double size)
{
  return RectByCenterXYAndSizeInMeters(center.x, center.y, size, size);
}

m2::RectD RectByCenterXYAndOffset(m2::PointD const & center, double offset)
{
  return {ClampX(center.x - offset), ClampY(center.y - offset),
          ClampX(center.x + offset), ClampY(center.y + offset)};
}

m2::RectD RectByCenterLatLonAndSizeInMeters(double lat, double lon, double size)
{
  return RectByCenterXYAndSizeInMeters(FromLatLon(lat, lon), size);
}

m2::RectD FromLatLon(m2::RectD const & rect)
{
  return { FromLatLon(rect.minY(), rect.minX()), FromLatLon(rect.maxY(), rect.maxX()) };
}

m2::RectD ToLatLon(m2::RectD const & rect)
{
  return { YToLat(rect.minY()), XToLon(rect.minX()), YToLat(rect.maxY()), XToLon(rect.maxX()) };
}
}  // namespace mercator
