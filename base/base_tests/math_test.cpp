#include "testing/testing.hpp"

#include "base/math.hpp"

#include <limits>

#include <boost/math/special_functions/next.hpp>

namespace math_test
{
using namespace math;

namespace
{
// Returns the next representable floating point value without using conversion to integer.
template <typename Float>
Float NextFloat(Float const x, int dir = 1)
{
  return boost::math::float_advance(x, dir);
}

template <typename Float>
void TestMaxULPs()
{
  for (unsigned int logMaxULPs = 0; logMaxULPs <= 8; ++logMaxULPs)
  {
    unsigned int const maxULPs = (1 << logMaxULPs) - 1;
    for (int base = -1; base <= 1; ++base)
    {
      for (int dir = -1; dir <= 1; dir += 2)
      {
        Float const x = base;
        Float y = x;
        for (unsigned int i = 0; i <= maxULPs; ++i)
        {
          TEST(AlmostEqualULPs(x, y, maxULPs), (x, y, maxULPs, x - y, dir));
          Float const nextY = NextFloat(y, dir);
          TEST_NOT_EQUAL(y, nextY, (i, base, dir));
          y = nextY;
        }
        TEST(!AlmostEqualULPs(x, y, maxULPs), (x, y, maxULPs, x - y));
      }
    }
  }
}
}  // namespace

UNIT_TEST(PowUInt)
{
  TEST_EQUAL(PowUint(3, 10), 59049, ());
}

UNIT_TEST(AlmostEqualULPs_double)
{
  TEST_ALMOST_EQUAL_ULPS(3.0, 3.0, ());
  TEST_ALMOST_EQUAL_ULPS(+0.0, -0.0, ());

  double constexpr eps = std::numeric_limits<double>::epsilon();
  double constexpr dmax = std::numeric_limits<double>::max();

  TEST_ALMOST_EQUAL_ULPS(1.0 + eps, 1.0, ());
  TEST_ALMOST_EQUAL_ULPS(1.0 - eps, 1.0, ());
  TEST_ALMOST_EQUAL_ULPS(1.0 - eps, 1.0 + eps, ());

  TEST_ALMOST_EQUAL_ULPS(dmax, dmax, ());
  TEST_ALMOST_EQUAL_ULPS(-dmax, -dmax, ());
  TEST_ALMOST_EQUAL_ULPS(dmax/2.0, dmax/2.0, ());
  TEST_ALMOST_EQUAL_ULPS(1.0/dmax, 1.0/dmax, ());
  TEST_ALMOST_EQUAL_ULPS(-1.0/dmax, -1.0/dmax, ());

  TEST(!AlmostEqualULPs(1.0, -1.0), ());
  TEST(!AlmostEqualULPs(2.0, -2.0), ());
  TEST(!AlmostEqualULPs(dmax, -dmax), ());
  TEST(!AlmostEqualULPs(0.0, eps), ());
}

UNIT_TEST(AlmostEqualULPs_float)
{
  TEST_ALMOST_EQUAL_ULPS(3.0f, 3.0f, ());
  TEST_ALMOST_EQUAL_ULPS(+0.0f, -0.0f, ());

  float const eps = std::numeric_limits<float>::epsilon();
  float const dmax = std::numeric_limits<float>::max();

  TEST_ALMOST_EQUAL_ULPS(1.0f + eps, 1.0f, ());
  TEST_ALMOST_EQUAL_ULPS(1.0f - eps, 1.0f, ());
  TEST_ALMOST_EQUAL_ULPS(1.0f - eps, 1.0f + eps, ());

  TEST_ALMOST_EQUAL_ULPS(dmax, dmax, ());
  TEST_ALMOST_EQUAL_ULPS(-dmax, -dmax, ());
  TEST_ALMOST_EQUAL_ULPS(dmax/2.0f, dmax/2.0f, ());
  TEST_ALMOST_EQUAL_ULPS(1.0f/dmax, 1.0f/dmax, ());
  TEST_ALMOST_EQUAL_ULPS(-1.0f/dmax, -1.0f/dmax, ());

  TEST(!AlmostEqualULPs(1.0f, -1.0f), ());
  TEST(!AlmostEqualULPs(2.0f, -2.0f), ());
  TEST(!AlmostEqualULPs(dmax, -dmax), ());
  TEST(!AlmostEqualULPs(0.0f, eps), ());
}

UNIT_TEST(AlmostEqual_Smoke)
{
  double constexpr small = 1e-18;
  double constexpr eps = 1e-10;

  TEST(AlmostEqualAbs(0.0, 0.0 + small, eps), ());
  TEST(!AlmostEqualRel(0.0, 0.0 + small, eps), ());
  TEST(!AlmostEqualULPs(0.0, 0.0 + small), ());

  TEST(AlmostEqualAbs(1.0, 1.0 + small, eps), ());
  TEST(AlmostEqualRel(1.0, 1.0 + small, eps), ());
  TEST(AlmostEqualULPs(1.0, 1.0 + small), ());

  TEST(AlmostEqualRel(123456789.0, 123456780.0, 1e-7), ());
}

UNIT_TEST(AlmostEqualULPs_MaxULPs_double)
{
  TestMaxULPs<double>();
}

UNIT_TEST(AlmostEqualULPs_MaxULPs_float)
{
  TestMaxULPs<float>();
}

UNIT_TEST(TEST_FLOAT_DOUBLE_EQUAL_macros)
{
  float const fx = 3;
  float const fy = NextFloat(NextFloat(NextFloat(fx)));
  TEST_ALMOST_EQUAL_ULPS(fx, fy, ());
  TEST_NOT_ALMOST_EQUAL_ULPS(fx, 2.0f, ());

  double const dx = 3;
  double const dy = NextFloat(NextFloat(NextFloat(dx)));
  TEST_ALMOST_EQUAL_ULPS(dx, dy, ());
  TEST_NOT_ALMOST_EQUAL_ULPS(dx, 2.0, ());
}

UNIT_TEST(GCD)
{
  TEST_EQUAL(GCD(6, 3), 3, ());
  TEST_EQUAL(GCD(14, 7), 7, ());
  TEST_EQUAL(GCD(100, 100), 100, ());
  TEST_EQUAL(GCD(7, 3), 1, ());
  TEST_EQUAL(GCD(8, 3), 1, ());
  TEST_EQUAL(GCD(9, 3), 3, ());
}

UNIT_TEST(LCM)
{
  TEST_EQUAL(LCM(6, 3), 6, ());
  TEST_EQUAL(LCM(14, 7), 14, ());
  TEST_EQUAL(LCM(100, 100), 100, ());
  TEST_EQUAL(LCM(7, 3), 21, ());
  TEST_EQUAL(LCM(8, 3), 24, ());
  TEST_EQUAL(LCM(9, 3), 9, ());
}

UNIT_TEST(Sign)
{
  TEST_EQUAL(1, Sign(1), ());
  TEST_EQUAL(1, Sign(10.4), ());

  TEST_EQUAL(0, Sign(0), ());
  TEST_EQUAL(0, Sign(0.0), ());

  TEST_EQUAL(-1, Sign(-11), ());
  TEST_EQUAL(-1, Sign(-10.4), ());
}
}  // namespace math_test