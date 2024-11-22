#include "testing/testing.hpp"

#include "coding/coding_tests/test_polylines.hpp"

#include "coding/point_coding.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"

#include <cmath>
#include <random>

using namespace std;

namespace
{
double const kEps = kMwmPointAccuracy;
uint8_t const kCoordBits = kPointCoordBits;
uint32_t const kBig = uint32_t{1} << 30;

void CheckEqualPoints(m2::PointD const & p1, m2::PointD const & p2)
{
  TEST(p1.EqualDxDy(p2, kEps), (p1, p2));

  TEST_GREATER_OR_EQUAL(p1.x, -180.0, ());
  TEST_GREATER_OR_EQUAL(p1.y, -180.0, ());
  TEST_LESS_OR_EQUAL(p1.x, 180.0, ());
  TEST_LESS_OR_EQUAL(p1.y, 180.0, ());

  TEST_GREATER_OR_EQUAL(p2.x, -180.0, ());
  TEST_GREATER_OR_EQUAL(p2.y, -180.0, ());
  TEST_LESS_OR_EQUAL(p2.x, 180.0, ());
  TEST_LESS_OR_EQUAL(p2.y, 180.0, ());
}
}  // namespace

UNIT_TEST(PointDToPointU_Epsilons)
{
  m2::PointD const arrPt[] = {{-180, -180}, {-180, 180}, {180, 180}, {180, -180}};
  m2::PointD const arrD[] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
  size_t const count = ARRAY_SIZE(arrPt);

  double eps = 1.0;
  while (true)
  {
    size_t i = 0;
    for (; i < count; ++i)
    {
      m2::PointU p0 = PointDToPointU(arrPt[i].x, arrPt[i].y, kCoordBits);
      m2::PointU p1 = PointDToPointU(arrPt[i].x + arrD[i].x * eps,
                                     arrPt[i].y + arrD[i].y * eps,
                                     kCoordBits);

      if (p0 != p1)
        break;
    }
    if (i == count)
      break;

    eps *= 0.1;
  }

  LOG(LINFO, ("Epsilon (relative error) =", eps));

  for (size_t i = 0; i < count; ++i)
  {
    m2::PointU const p1 = PointDToPointU(arrPt[i].x, arrPt[i].y, kCoordBits);
    m2::PointU const p2(p1.x + arrD[i].x, p1.y + arrD[i].y);
    m2::PointD const p3 = PointUToPointD(p2, kCoordBits);

    LOG(LINFO, ("Dx =", p3.x - arrPt[i].x, "Dy =", p3.y - arrPt[i].y));
  }
}

UNIT_TEST(PointDToPointU_WithLimitRect)
{
  mt19937 rng(0);

  m2::PointD const limitRectOrigin[] = {{0.0, 0.0}, {10.0, 10.0}, {90.0, 90.0}, {160.0, 160.0}};
  double const limitRectSize[] = {0.1, 1.0, 5.0, 10.0, 20.0};
  size_t const pointsPerRect = 100;

  for (auto const & origin : limitRectOrigin)
  {
    for (auto const sizeX : limitRectSize)
    {
      for (auto const sizeY : limitRectSize)
      {
        m2::RectD const limitRect(origin.x, origin.y, origin.x + sizeX, origin.y + sizeY);
        auto distX = uniform_real_distribution<double>(limitRect.minX(), limitRect.maxX());
        auto distY = uniform_real_distribution<double>(limitRect.minY(), limitRect.maxY());
        auto const coordBits = GetCoordBits(limitRect, kEps);
        TEST_NOT_EQUAL(coordBits, 0, ());
        // All rects in this test are more than 2 times smaller than mercator range.
        TEST_LESS(coordBits, kCoordBits, (limitRect));
        for (size_t i = 0; i < pointsPerRect; ++i)
        {
          auto const pt = m2::PointD(distX(rng), distY(rng));
          auto const pointU = PointDToPointU(pt, coordBits, limitRect);
          auto const pointD = PointUToPointD(pointU, coordBits, limitRect);
          TEST(AlmostEqualAbs(pt, pointD, kEps), (limitRect, pt, pointD, coordBits, kEps));
        }
      }
    }
  }
}

UNIT_TEST(PointToInt64Obsolete_Smoke)
{
  m2::PointD const arr[] = {{1.25, 1.3}, {180, 90}, {-180, -90}, {0, 0}};

  for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
    CheckEqualPoints(arr[i],
                     Int64ToPointObsolete(PointToInt64Obsolete(arr[i], kCoordBits), kCoordBits));
}

UNIT_TEST(PointToInt64Obsolete_Grid)
{
  int const delta = 5;
  for (int ix = -180; ix <= 180; ix += delta)
  {
    for (int iy = -180; iy <= 180; iy += delta)
    {
      m2::PointD const pt(ix, iy);
      int64_t const id = PointToInt64Obsolete(pt, kCoordBits);
      m2::PointD const pt1 = Int64ToPointObsolete(id, kCoordBits);

      CheckEqualPoints(pt, pt1);

      int64_t const id1 = PointToInt64Obsolete(pt1, kCoordBits);
      TEST_EQUAL(id, id1, (pt, pt1));
    }
  }
}

UNIT_TEST(PointToInt64Obsolete_Bounds)
{
  double const arrEps[] = {-1.0E-2, -1.0E-3, -1.0E-4, 0, 1.0E-4, 1.0E-3, 1.0E-2};

  m2::PointD const arrPt[] = {{0, 0},     {-180, -180}, {-180, 180}, {180, 180}, {180, -180},
                              {-90, -90}, {-90, 90},    {90, 90},    {90, -90}};

  for (size_t iP = 0; iP < ARRAY_SIZE(arrPt); ++iP)
  {
    for (size_t iX = 0; iX < ARRAY_SIZE(arrEps); ++iX)
    {
      for (size_t iY = 0; iY < ARRAY_SIZE(arrEps); ++iY)
      {
        m2::PointD const pt(arrPt[iP].x + arrEps[iX], arrPt[iP].y + arrEps[iY]);
        m2::PointD const pt1 =
            Int64ToPointObsolete(PointToInt64Obsolete(pt, kCoordBits), kCoordBits);

        TEST(fabs(pt.x - pt1.x) <= (fabs(arrEps[iX]) + kEps) &&
                 fabs(pt.y - pt1.y) <= (fabs(arrEps[iY]) + kEps),
             (pt, pt1));
      }
    }
  }
}

UNIT_TEST(PointUToUint64Obsolete_0)
{
  TEST_EQUAL(0, PointUToUint64Obsolete(m2::PointU(0, 0)), ());
  TEST_EQUAL(m2::PointU(0, 0), Uint64ToPointUObsolete(0), ());
}

UNIT_TEST(PointUToUint64Obsolete_Interlaced)
{
  TEST_EQUAL(0xAAAAAAAAAAAAAAAAULL, PointUToUint64Obsolete(m2::PointU(0, 0xFFFFFFFF)), ());
  TEST_EQUAL(0x5555555555555555ULL, PointUToUint64Obsolete(m2::PointU(0xFFFFFFFF, 0)), ());
  TEST_EQUAL(0xAAAAAAAAAAAAAAA8ULL, PointUToUint64Obsolete(m2::PointU(0, 0xFFFFFFFE)), ());
  TEST_EQUAL(0x5555555555555554ULL, PointUToUint64Obsolete(m2::PointU(0xFFFFFFFE, 0)), ());
}

UNIT_TEST(PointUToUint64Obsolete_1bit)
{
  TEST_EQUAL(2, PointUToUint64Obsolete(m2::PointU(0, 1)), ());
  TEST_EQUAL(m2::PointU(0, 1), Uint64ToPointUObsolete(2), ());
  TEST_EQUAL(1, PointUToUint64Obsolete(m2::PointU(1, 0)), ());
  TEST_EQUAL(m2::PointU(1, 0), Uint64ToPointUObsolete(1), ());

  TEST_EQUAL(3ULL << 60, PointUToUint64Obsolete(m2::PointU(kBig, kBig)), ());
  TEST_EQUAL((1ULL << 60) - 1, PointUToUint64Obsolete(m2::PointU(kBig - 1, kBig - 1)), ());
}

UNIT_TEST(PointToInt64Obsolete_DataSet1)
{
  using namespace geometry_coding_tests;

  for (size_t i = 0; i < ARRAY_SIZE(arr1); ++i)
  {
    m2::PointD const pt(arr1[i].x, arr1[i].y);
    int64_t const id = PointToInt64Obsolete(pt, kCoordBits);
    m2::PointD const pt1 = Int64ToPointObsolete(id, kCoordBits);

    CheckEqualPoints(pt, pt1);

    int64_t const id1 = PointToInt64Obsolete(pt1, kCoordBits);
    TEST_EQUAL(id, id1, (pt, pt1));
  }
}
