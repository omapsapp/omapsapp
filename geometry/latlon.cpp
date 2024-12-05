#include "latlon.hpp"

#include "base/math.hpp"

#include <sstream>
#include <tuple>

namespace ms
{
bool LatLon::operator==(ms::LatLon const & rhs) const { return m_lat == rhs.m_lat && m_lon == rhs.m_lon; }

bool LatLon::operator<(ms::LatLon const & rhs) const
{
  return std::tie(m_lat, m_lon) < std::tie(rhs.m_lat, rhs.m_lon);
}

bool LatLon::EqualDxDy(LatLon const & p, double eps) const
{
  return (::AlmostEqualAbs(m_lat, p.m_lat, eps) && ::AlmostEqualAbs(m_lon, p.m_lon, eps));
}

std::string DebugPrint(LatLon const & t)
{
  std::ostringstream out;
  out.precision(9); // <3>.<6> digits is enough here
  out << "ms::LatLon(" << t.m_lat << ", " << t.m_lon << ")";
  return out.str();
}

bool AlmostEqualAbs(LatLon const & ll1, LatLon const & ll2, double const & eps)
{
  return ::AlmostEqualAbs(ll1.m_lat, ll2.m_lat, eps) &&
         ::AlmostEqualAbs(ll1.m_lon, ll2.m_lon, eps);
}
}  // namespace ms
