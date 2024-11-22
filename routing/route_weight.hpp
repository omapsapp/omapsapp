#pragma once

#include "routing/base/astar_weight.hpp"

#include "base/math.hpp"

#include <cstdint>
#include <iostream>
#include <limits>

namespace routing
{
class RouteWeight final
{
public:
  RouteWeight() = default;

  explicit constexpr RouteWeight(double weight) : m_weight(weight) {}

  constexpr RouteWeight(double weight, int8_t numPassThroughChanges, int8_t numAccessChanges,
                        int8_t numAccessConditionalPenalties, double transitTime)
    : m_weight(weight)
    , m_numPassThroughChanges(numPassThroughChanges)
    , m_numAccessChanges(numAccessChanges)
    , m_numAccessConditionalPenalties(numAccessConditionalPenalties)
    , m_transitTime(transitTime)
  {
  }

  static RouteWeight FromCrossMwmWeight(double weight) { return RouteWeight(weight); }

  double ToCrossMwmWeight() const;
  double GetWeight() const { return m_weight; }
  int8_t GetNumPassThroughChanges() const { return m_numPassThroughChanges; }
  double GetTransitTime() const { return m_transitTime; }

  double GetIntegratedWeight() const;
  bool operator<(RouteWeight const & rhs) const;

  bool operator==(RouteWeight const & rhs) const { return !((*this) < rhs) && !(rhs < (*this)); }

  bool operator!=(RouteWeight const & rhs) const { return !((*this) == rhs); }

  bool operator>(RouteWeight const & rhs) const { return rhs < (*this); }

  bool operator>=(RouteWeight const & rhs) const { return !((*this) < rhs); }

  bool operator<=(RouteWeight const & rhs) const { return rhs >= (*this); }

  RouteWeight operator+(RouteWeight const & rhs) const;

  RouteWeight operator-(RouteWeight const & rhs) const;

  RouteWeight & operator+=(RouteWeight const & rhs);

  RouteWeight operator-() const
  {
    ASSERT_NOT_EQUAL(m_numPassThroughChanges, std::numeric_limits<int8_t>::min(), ());
    ASSERT_NOT_EQUAL(m_numAccessChanges, std::numeric_limits<int8_t>::min(), ());
    ASSERT_NOT_EQUAL(m_numAccessConditionalPenalties, std::numeric_limits<int8_t>::min(), ());
    return RouteWeight(-m_weight, -m_numPassThroughChanges, -m_numAccessChanges,
                       -m_numAccessConditionalPenalties, -m_transitTime);
  }

  bool IsAlmostEqualForTests(RouteWeight const & rhs, double epsilon) const
  {
    return m_numPassThroughChanges == rhs.m_numPassThroughChanges &&
           m_numAccessChanges == rhs.m_numAccessChanges &&
           m_numAccessConditionalPenalties == rhs.m_numAccessConditionalPenalties &&
           AlmostEqualAbs(m_weight, rhs.m_weight, epsilon) &&
           AlmostEqualAbs(m_transitTime, rhs.m_transitTime, epsilon);
  }

  friend std::ostream & operator<<(std::ostream & os, RouteWeight const & routeWeight);

  friend RouteWeight operator*(double lhs, RouteWeight const & rhs);

  /// @name Penalties in seconds. Actual value see in cpp.
  /// Made as public-static to change values in routing index generator.
  /// @todo Consider put this settings in some "configure" profile.
  /// @{
  static int s_PassThroughPenaltyS;
  static int s_AccessPenaltyS;
  /// @}

private:
  // Regular weight (seconds).
  double m_weight = 0.0;
  // Number of pass-through/non-pass-through zone changes.
  int8_t m_numPassThroughChanges = 0;
  // Number of access=yes/access={private,destination} zone changes.
  int8_t m_numAccessChanges = 0;
  // Number of access:conditional dangerous zones (when RoadAccess::GetAccess() return
  // |Confidence::Maybe|).
  int8_t m_numAccessConditionalPenalties = 0;
  // Transit time. It's already included in |m_weight| (m_transitTime <= m_weight).
  double m_transitTime = 0.0;
};

template <>
constexpr RouteWeight GetAStarWeightMax<RouteWeight>()
{
  return RouteWeight(std::numeric_limits<double>::max() /* weight */,
                     std::numeric_limits<int8_t>::max() /* numPassThroughChanges */,
                     std::numeric_limits<int8_t>::max() /* numAccessChanges */,
                     std::numeric_limits<int8_t>::max() /* numAccessConditionalPenalties */,
                     0.0 /* transitTime */);  // operator< prefers bigger transit time
}

template <>
constexpr RouteWeight GetAStarWeightZero<RouteWeight>()
{
  return RouteWeight(0.0 /* weight */, 0 /* numPassThroughChanges */, 0 /* numAccessChanges */,
                     0 /* numAccessConditionalPenalties */, 0.0 /* transitTime */);
}
}  // namespace routing
