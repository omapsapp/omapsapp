#include "geometry/geometry_tests/equality.hpp"

#include "base/macros.hpp"

#include "testing/testing.hpp"

#include "geometry/angles.hpp"


using namespace test;
using math::pi;

UNIT_TEST(Atan)
{
  double const h4 = math::pi/4.0;

  TEST(is_equal_atan(1, 1, h4), ());
  TEST(is_equal_atan(-1, 1, math::pi - h4), ());
  TEST(is_equal_atan(-1, -1, h4 - math::pi), ());
  TEST(is_equal_atan(1, -1, -h4), ());

  double const hh = atan(1.0/2.0);

  TEST(is_equal_atan(2, 1, hh), ());
  TEST(is_equal_atan(-2, 1, math::pi - hh), ());
  TEST(is_equal_atan(-2, -1, hh - math::pi), ());
  TEST(is_equal_atan(2, -1, -hh), ());
}

UNIT_TEST(Atan2)
{
  TEST_ALMOST_EQUAL_ULPS(atan2(1, 0), pi/2.0, ());
  TEST_ALMOST_EQUAL_ULPS(atan2(-1, 0), -pi/2.0, ());
  TEST_ALMOST_EQUAL_ULPS(atan2(0, 1), 0.0, ());
  TEST_ALMOST_EQUAL_ULPS(atan2(0, -1), pi, ());

  TEST_ALMOST_EQUAL_ULPS(atan2(1, 1), pi/4.0, ());
}

namespace
{
  void check_avg(double arr[], size_t n, double v)
  {
    ang::AverageCalc calc;
    for (size_t i = 0; i < n; ++i)
      calc.Add(arr[i]);

    double const avg = calc.GetAverage();
    TEST(is_equal_angle(avg, v), (avg, v));
  }
}

UNIT_TEST(Average)
{
  double constexpr eps = 1.0E-3;

  double arr1[] = { math::pi-eps, -math::pi+eps };
  TEST(is_equal_angle(ang::GetMiddleAngle(arr1[0], arr1[1]), math::pi), ());
  check_avg(arr1, ARRAY_SIZE(arr1), math::pi);

  double arr2[] = { eps, -eps };
  TEST(is_equal_angle(ang::GetMiddleAngle(arr2[0], arr2[1]), 0.0), ());
  check_avg(arr2, ARRAY_SIZE(arr2), 0.0);
}

UNIT_TEST(ShortestDistance)
{
  TEST_ALMOST_EQUAL_ULPS(ang::GetShortestDistance(0, math::pi), math::pi, ());
  TEST_ALMOST_EQUAL_ULPS(ang::GetShortestDistance(0, math::pi + 1), -math::pi + 1, ());

  TEST_ALMOST_EQUAL_ULPS(ang::GetShortestDistance(math::pi - 1, 0), -math::pi + 1, ());
  TEST_ALMOST_EQUAL_ULPS(ang::GetShortestDistance(math::pi + 1, 0), math::pi - 1, ());
}

UNIT_TEST(TwoVectorsAngle)
{
  double constexpr eps = 1e-10;
  TEST(AlmostEqualAbs(ang::TwoVectorsAngle(m2::Point<double>(0, 0)  /* p */,
                                                 m2::Point<double>(0, 1)  /* p1 */,
                                                 m2::Point<double>(1, 0)) /* p2 */, 3 * math::pi2, eps), ());
  TEST(AlmostEqualAbs(ang::TwoVectorsAngle(m2::Point<double>(1, 1)  /* p */,
                                                 m2::Point<double>(2, 2)  /* p1 */,
                                                 m2::Point<double>(1, 2)) /* p2 */, math::pi4, eps), ());
  TEST(AlmostEqualAbs(ang::TwoVectorsAngle(m2::Point<double>(0, 0)  /* p */,
                                                 m2::Point<double>(1, 0)  /* p1 */,
                                                 m2::Point<double>(0, -1)) /* p2 */, 3 * math::pi2, eps), ());
  TEST(AlmostEqualAbs(ang::TwoVectorsAngle(m2::Point<double>(0, 0)  /* p */,
                                                 m2::Point<double>(1, 0)  /* p1 */,
                                                 m2::Point<double>(-1, 0)) /* p2 */, math::pi, eps), ());
}

UNIT_TEST(Azimuth)
{
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(-1, 0),
                                   m2::Point<double>(0, 1),
                                   math::pi2), -math::pi4), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(-1, 0),
                                   m2::Point<double>(0, 1),
                                   0.0), math::pi4), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(-1, 1),
                                   m2::Point<double>(1, -1),
                                   0.0), 3 * math::pi4), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(1, 1),
                                   m2::Point<double>(0, 1),
                                   -math::pi2), 0.0), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(1, -1),
                                   m2::Point<double>(-1, -1),
                                   math::pi), math::pi2), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(0, 0),
                                   m2::Point<double>(-1, -1),
                                   math::pi4), math::pi), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(0.5, -0.5),
                                   m2::Point<double>(-0.5, 0.5),
                                   math::pi4), -math::pi2), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(0.1, 0.1),
                                   m2::Point<double>(0.2, 0.2),
                                   math::pi4), 0.0), ());
  TEST(is_equal_angle(ang::Azimuth(m2::Point<double>(0.7, 0.7),
                                   m2::Point<double>(-0.2, -0.2),
                                   math::pi2), 3 * math::pi4), ());
}
