#pragma once

#include "geometry/point2d.hpp"

#include <limits>
#include <string>
#include <vector>

namespace geometry
{
using Altitude = int16_t;
using Altitudes = std::vector<Altitude>;

Altitude constexpr kInvalidAltitude = std::numeric_limits<Altitude>::min();
Altitude constexpr kDefaultAltitudeMeters = 0;

class PointWithAltitude
{
public:
  PointWithAltitude();
  PointWithAltitude(m2::PointD const & point, Altitude altitude = kDefaultAltitudeMeters);
  operator m2::PointD() const { return m_point; }

  bool operator==(PointWithAltitude const & r) const;
  bool operator!=(PointWithAltitude const & r) const { return !(*this == r); }
  bool operator<(PointWithAltitude const & r) const;

  m2::PointD const & GetPoint() const { return m_point; }
  Altitude GetAltitude() const { return m_altitude; }

  void SetPoint(m2::PointD const & point) { m_point = point; }
  void SetAltitude(Altitude altitude) { m_altitude = altitude; }

private:
  friend std::string DebugPrint(PointWithAltitude const & r);

  m2::PointD m_point;
  Altitude m_altitude;
};

std::string DebugPrint(PointWithAltitude const & r);

template <typename T>
m2::Point<T> GetPoint(m2::Point<T> const & point) { return point; }
inline m2::PointD GetPoint(PointWithAltitude const & pwa) { return pwa.GetPoint(); }

PointWithAltitude MakePointWithAltitudeForTesting(m2::PointD const & point);

bool AlmostEqualAbs(PointWithAltitude const & lhs, PointWithAltitude const & rhs, double eps);
double Distance(PointWithAltitude const & p1, PointWithAltitude const & p2);
}  // namespace geometry

namespace std
{
template <>
struct hash<geometry::PointWithAltitude>
{
  size_t operator()(geometry::PointWithAltitude const & point) const;
};
}  // namespace std
