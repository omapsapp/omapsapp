#pragma once

#include "geometry/latlon.hpp"
#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/math.hpp"

namespace mercator
{
// Use to compare/match lat lon coordinates.
static double constexpr kPointEqualityEps = 1e-7;
struct Bounds
{
  static double constexpr kMinX = -180.0;
  static double constexpr kMaxX = 180.0;
  static double constexpr kMinY = -180.0;
  static double constexpr kMaxY = 180.0;
  static double constexpr kRangeX = kMaxX - kMinX;
  static double constexpr kRangeY = kMaxY - kMinY;

  // The denominator is the Earth circumference at the Equator in meters.
  // The value is a bit off for some reason; 40075160 seems to be correct.
  static double constexpr kDegreesInMeter = 360.0 / 40008245.0;
  static double constexpr kMetersInDegree = 40008245.0 / 360.0;

  static m2::RectD FullRect()
  {
    return m2::RectD(kMinX, kMinY, kMaxX, kMaxY);
  }
};

inline bool ValidLon(double d) { return math::Between(-180.0, 180.0, d); }
inline bool ValidLat(double d) { return math::Between(-90.0, 90.0, d); }

inline bool ValidX(double d) { return math::Between(Bounds::kMinX, Bounds::kMaxX, d); }
inline bool ValidY(double d) { return math::Between(Bounds::kMinY, Bounds::kMaxY, d); }

inline double ClampX(double d) { return math::Clamp(d, Bounds::kMinX, Bounds::kMaxX); }
inline double ClampY(double d) { return math::Clamp(d, Bounds::kMinY, Bounds::kMaxY); }

void ClampPoint(m2::PointD & pt);

double YToLat(double y);
double LatToY(double lat);

inline double XToLon(double x) { return x; }
inline double LonToX(double lon) { return lon; }

inline double MetersToMercator(double meters) { return meters * Bounds::kDegreesInMeter; }
inline double MercatorToMeters(double mercator) { return mercator * Bounds::kMetersInDegree; }

/// @name Get rect for center point (lon, lat) and dimensions in meters.
/// @return mercator rect.
m2::RectD MetersToXY(double lon, double lat, double lonMetersR, double latMetersR);
m2::RectD MetersToXY(double lon, double lat, double metersR);

m2::RectD RectByCenterXYAndSizeInMeters(double centerX, double centerY, double sizeX, double sizeY);

m2::RectD RectByCenterXYAndSizeInMeters(m2::PointD const & center, double size);

m2::RectD RectByCenterXYAndOffset(m2::PointD const & center, double offset);

m2::PointD GetSmPoint(m2::PointD const & pt, double lonMetersR, double latMetersR);

inline m2::PointD FromLatLon(double lat, double lon) { return m2::PointD(LonToX(lon), LatToY(lat)); }
inline m2::PointD FromLatLon(ms::LatLon const & point) { return FromLatLon(point.m_lat, point.m_lon); }

m2::RectD RectByCenterLatLonAndSizeInMeters(double lat, double lon, double size);

inline ms::LatLon ToLatLon(m2::PointD const & point) { return {YToLat(point.y), XToLon(point.x)}; }

m2::RectD FromLatLon(m2::RectD const & rect);
m2::RectD ToLatLon(m2::RectD const & rect);

/// Calculates distance on Earth in meters between two mercator points.
double DistanceOnEarth(m2::PointD const & p1, m2::PointD const & p2);

/// Calculates area of a triangle on Earth in m² by three mercator points.
double AreaOnEarth(m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3);
/// Calculates area on Earth in m².
double AreaOnEarth(m2::RectD const & mercatorRect);
}  // namespace mercator
