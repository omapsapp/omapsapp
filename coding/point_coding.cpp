#include "coding/point_coding.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/bits.hpp"

#include <algorithm>

namespace
{
double CoordSize(uint8_t coordBits)
{
  ASSERT(coordBits >= 1 && coordBits <= 32, (coordBits));
  return static_cast<double>((uint64_t{1} << coordBits) - 1);
}
}  // namespace

uint32_t DoubleToUint32(double x, double min, double max, uint8_t coordBits)
{
  ASSERT_LESS_OR_EQUAL(min, max, ());

  double const coordSize = CoordSize(coordBits);

  // Expand checks to avoid NANs when min == max.
  double d;
  if (x <= min)
    d = 0;
  else if (x >= max)
    d = coordSize;
  else
    d = (x - min) / (max - min) * coordSize;

  // Check in case of NANs.
  ASSERT(d >= 0 && d <= coordSize, (d, x, min, max, coordBits));
  return static_cast<uint32_t>(0.5 + d);
}

double Uint32ToDouble(uint32_t x, double min, double max, uint8_t coordBits)
{
  ASSERT_LESS_OR_EQUAL(min, max, ());

  double const coordSize = CoordSize(coordBits);
  auto const d = min + static_cast<double>(x) * (max - min) / coordSize;

  // It doesn't work now because of fancy serialization of m2::DiamondBox.
  /// @todo Check PathsThroughLayers search test. Refactor CitiesBoundariesSerDes.
  //ASSERT_LESS_OR_EQUAL(x, coordSize, (d, min, max, coordBits));

  // It doesn't work because of possible floating errors.
  //ASSERT(d >= min && d <= max, (d, x, min, max, coordBits));

  return math::Clamp(d, min, max);
}

m2::PointU PointDToPointU(double x, double y, uint8_t coordBits)
{
  using mercator::Bounds;
  return { DoubleToUint32(x, Bounds::kMinX, Bounds::kMaxX, coordBits),
           DoubleToUint32(y, Bounds::kMinY, Bounds::kMaxY, coordBits) };
}

m2::PointU PointDToPointU(m2::PointD const & pt, uint8_t coordBits)
{
  return PointDToPointU(pt.x, pt.y, coordBits);
}

m2::PointU PointDToPointU(m2::PointD const & pt, uint8_t coordBits, m2::RectD const & limitRect)
{
  return { DoubleToUint32(pt.x, limitRect.minX(), limitRect.maxX(), coordBits),
           DoubleToUint32(pt.y, limitRect.minY(), limitRect.maxY(), coordBits) };
}

m2::PointD PointUToPointD(m2::PointU const & pt, uint8_t coordBits)
{
  using mercator::Bounds;
  return { Uint32ToDouble(pt.x, Bounds::kMinX, Bounds::kMaxX, coordBits),
           Uint32ToDouble(pt.y, Bounds::kMinY, Bounds::kMaxY, coordBits) };
}

m2::PointD PointUToPointD(m2::PointU const & pt, uint8_t coordBits, m2::RectD const & limitRect)
{
  return { Uint32ToDouble(pt.x, limitRect.minX(), limitRect.maxX(), coordBits),
           Uint32ToDouble(pt.y, limitRect.minY(), limitRect.maxY(), coordBits) };
}

uint8_t GetCoordBits(m2::RectD const & limitRect, double accuracy)
{
  auto const range = std::max(limitRect.SizeX(), limitRect.SizeY());
  auto const valuesNumber = 1.0 + range / accuracy;
  for (uint8_t coordBits = 1; coordBits <= 32; ++coordBits)
  {
    if (CoordSize(coordBits) >= valuesNumber)
      return coordBits;
  }
  return 0;
}

// Obsolete functions ------------------------------------------------------------------------------

int64_t PointToInt64Obsolete(double x, double y, uint8_t coordBits)
{
  int64_t const res = static_cast<int64_t>(PointUToUint64Obsolete(PointDToPointU(x, y, coordBits)));

  ASSERT_GREATER_OR_EQUAL(res, 0, ("Highest bits of (ix, iy) are not used, so res should be > 0."));
  ASSERT_LESS_OR_EQUAL(static_cast<uint64_t>(res), uint64_t{3} << 2 * kPointCoordBits, ());
  return res;
}

int64_t PointToInt64Obsolete(m2::PointD const & pt, uint8_t coordBits)
{
  return PointToInt64Obsolete(pt.x, pt.y, coordBits);
}

m2::PointD Int64ToPointObsolete(int64_t v, uint8_t coordBits)
{
  ASSERT_GREATER_OR_EQUAL(v, 0, ("Highest bits of (ix, iy) are not used, so res should be > 0."));
  ASSERT_LESS_OR_EQUAL(static_cast<uint64_t>(v), uint64_t{3} << 2 * kPointCoordBits, ());
  return PointUToPointD(Uint64ToPointUObsolete(static_cast<uint64_t>(v)), coordBits);
}

std::pair<int64_t, int64_t> RectToInt64Obsolete(m2::RectD const & r, uint8_t coordBits)
{
  int64_t const p1 = PointToInt64Obsolete(r.minX(), r.minY(), coordBits);
  int64_t const p2 = PointToInt64Obsolete(r.maxX(), r.maxY(), coordBits);
  return std::make_pair(p1, p2);
}

m2::RectD Int64ToRectObsolete(std::pair<int64_t, int64_t> const & p, uint8_t coordBits)
{
  m2::PointD const pt1 = Int64ToPointObsolete(p.first, coordBits);
  m2::PointD const pt2 = Int64ToPointObsolete(p.second, coordBits);
  return m2::RectD(pt1, pt2);
}

uint64_t PointUToUint64Obsolete(m2::PointU const & pt)
{
  uint64_t const res = bits::BitwiseMerge(pt.x, pt.y);
  ASSERT_EQUAL(pt, Uint64ToPointUObsolete(res), ());
  return res;
}

m2::PointU Uint64ToPointUObsolete(int64_t v)
{
  m2::PointU res;
  bits::BitwiseSplit(v, res.x, res.y);
  return res;
}
