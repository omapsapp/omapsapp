#pragma once

#include "coding/geometry_coding.hpp"

#include "geometry/parametrized_segment.hpp"
#include "geometry/point2d.hpp"
#include "geometry/simplification.hpp"

#include "indexer/feature_data.hpp"
#include "indexer/scales.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"

#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace feature
{
class FeatureBuilder;

class CalculateMidPoints
{
public:
  using CellAndOffset = std::pair<uint64_t, uint64_t>;
  using MinDrawableScaleFn = std::function<int (FeatureBuilder const & fb)>;

  CalculateMidPoints();

  void operator()(FeatureBuilder const & ft, uint64_t pos);
  bool operator()(m2::PointD const & p);

  m2::PointD GetCenter() const;
  std::vector<CellAndOffset> const & GetVector() const { return m_vec; }

  void Sort();

private:
  m2::PointD m_midLoc;
  m2::PointD m_midAll = m2::PointD::Zero();
  size_t m_locCount = 0;
  size_t m_allCount = 0;
  uint8_t m_coordBits = serial::GeometryCodingParams().GetCoordBits();
  MinDrawableScaleFn m_minDrawableScaleFn;
  std::vector<CellAndOffset> m_vec;
};

template <typename Point>
inline bool ArePointsEqual(Point const & p1, Point const & p2)
{
  return p1 == p2;
}

template <>
inline bool ArePointsEqual<m2::PointD>(m2::PointD const & p1, m2::PointD const & p2)
{
  return AlmostEqualULPs(p1, p2);
}

class DistanceToSegmentWithRectBounds
{
public:
  explicit DistanceToSegmentWithRectBounds(m2::RectD const & rect) : m_rect(rect) {}

  // Returns squared distance from the segment [a, b] to the point p unless
  // p is close to the borders of |m_rect|, in which case returns a very large number.
  double operator()(m2::PointD const & a, m2::PointD const & b, m2::PointD const & p) const
  {
    if (AlmostEqualAbs(p.x, m_rect.minX(), m_eps) ||
        AlmostEqualAbs(p.x, m_rect.maxX(), m_eps) ||
        AlmostEqualAbs(p.y, m_rect.minY(), m_eps) ||
        AlmostEqualAbs(p.y, m_rect.maxY(), m_eps))
    {
      // Points near rect should be in a result simplified vector.
      return std::numeric_limits<double>::max();
    }

    return m2::SquaredDistanceFromSegmentToPoint()(a, b, p);
  }

  double GetEpsilon() const { return m_eps; }

private:
  m2::RectD const & m_rect;

  // 5.0E-7 is near with minimal epsilon when integer points are different
  // PointDToPointU(x, y) != PointDToPointU(x + m_eps, y + m_eps)
  double m_eps = 5.0E-7;
};

template <typename DistanceFn, typename PointsContainer>
void SimplifyPoints(DistanceFn distFn, int level, PointsContainer const & in, PointsContainer & out)
{
  if (in.size() < 2)
    return;

  double const eps = math::Pow2(scales::GetEpsilonForSimplify(level));

  SimplifyNearOptimal(20, in.begin(), in.end(), eps, distFn,
                      AccumulateSkipSmallTrg<DistanceFn, m2::PointD>(distFn, out, eps));

  CHECK_GREATER(out.size(), 1, ());
  CHECK(ArePointsEqual(in.front(), out.front()), ());
  CHECK(ArePointsEqual(in.back(), out.back()), ());
}
}  // namespace feature
