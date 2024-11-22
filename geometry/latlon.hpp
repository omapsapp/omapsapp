#pragma once

#include "base/math.hpp"

#include <string>

namespace ms
{
/// \brief Class for representing WGS point.
class LatLon
{
public:
  static double constexpr kMinLat = -90.0;
  static double constexpr kMaxLat = 90.0;
  static double constexpr kMinLon = -180.0;
  static double constexpr kMaxLon = 180.0;
  static double constexpr kInvalid = -1000.0;

  // Default values are invalid.
  double m_lat = kInvalid;
  double m_lon = kInvalid;

  constexpr LatLon() = default;
  constexpr LatLon(double lat, double lon) : m_lat(lat), m_lon(lon) {}

  static constexpr LatLon Invalid() { return LatLon(kInvalid, kInvalid); }
  static constexpr LatLon Zero() { return LatLon(0.0, 0.0); }

  bool constexpr IsValid() const { return m_lat != kInvalid && m_lon != kInvalid; }
  bool operator==(LatLon const & rhs) const;
  bool operator<(LatLon const & rhs) const;

  bool EqualDxDy(LatLon const & p, double eps) const;

  struct Hash
  {
    size_t operator()(ms::LatLon const & p) const { return math::Hash(p.m_lat, p.m_lon); }
  };
};

bool AlmostEqualAbs(LatLon const & ll1, LatLon const & ll2, double const & eps);

std::string DebugPrint(LatLon const & t);
}  // namespace ms
