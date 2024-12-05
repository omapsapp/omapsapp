#pragma once

#include "geometry/point2d.hpp"

#include "base/math.hpp"

#include <cmath>
#include <limits>

namespace m2
{
// This class holds a parametrization of the
// line segment between two points p0 and p1.
// The parametrization is of the form
//   p(t) = p0 + t * dir.
// Other conditions:
//   dir is the normalized (p1 - p0) vector.
//   length(dir) = 1.
//   p(0) = p0.
//   p(T) = p1 with T = length(p1 - p0).
//
// The points with t in [0, T] are the points of the segment.
template <typename Point>
class ParametrizedSegment
{
public:
  static_assert(std::numeric_limits<typename Point::value_type>::is_signed,
                "Unsigned points are not supported");

  ParametrizedSegment(Point const & p0, Point const & p1) : m_p0(p0), m_p1(p1)
  {
    m_d = m_p1 - m_p0;
    m_length = m_d.Length();
    if (m_d.IsAlmostZero())
      m_d = Point::Zero();
    else
      m_d = m_d / m_length;
  }

  // Returns the squared (euclidean) distance from the segment to |p|.
  double SquaredDistanceToPoint(Point const & p) const
  {
    m2::PointD const diff(p - m_p0);
    double const t = DotProduct(m_d, diff);

    if (t <= 0)
      return diff.SquaredLength();

    if (t >= m_length)
      return (p - m_p1).SquaredLength();

    // Closest point is between |m_p0| and |m_p1|.
    return math::Pow2(CrossProduct(diff, m_d));
  }

  // Returns the point of the segment that is closest to |p|.
  m2::PointD ClosestPointTo(Point const & p) const
  {
    m2::PointD const diff(p - m_p0);
    double const t = DotProduct(m_d, diff);

    if (t <= 0)
      return m_p0;

    if (t >= m_length)
      return m_p1;

    return m_p0 + m_d * t;
  }

  Point const & GetP0() const { return m_p0; }
  Point const & GetP1() const { return m_p1; }

private:
  Point m_p0;
  Point m_p1;
  m2::PointD m_d;
  double m_length;
};

// This functor is here only for backward compatibility. It is not obvious
// when looking at a call site whether x should be the first or the last parameter to the fuction.
// For readability, consider creating a parametrized segment and using its methods instead
// of using this functor.
struct SquaredDistanceFromSegmentToPoint
{
  /// @return Squared distance from the segment [a, b] to the point x.
  double operator()(m2::PointD const & a, m2::PointD const & b, m2::PointD const & x) const
  {
    ParametrizedSegment<m2::PointD> segment(a, b);
    return segment.SquaredDistanceToPoint(x);
  }

  template <class PointT>
  double operator()(PointT const & a, PointT const & b, PointT const & x) const
  {
    return this->operator()(m2::PointD(a), m2::PointD(b), m2::PointD(x));
  }
};
}  // namespace m2
