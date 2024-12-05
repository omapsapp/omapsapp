#include "testing/testing.hpp"

#include "geometry/point3d.hpp"

namespace point3d_tests
{
UNIT_TEST(Point3d_DotProduct)
{
  m3::Point<int> p1(1, 4, 3);
  m3::Point<int> p2(1, 2, 3);
  m3::Point<int> p3(1, 0, 3);

  TEST_EQUAL(m3::DotProduct(p1, p2), 18, ());
  TEST_EQUAL(m3::DotProduct(p2, p3), 10, ());
  TEST_EQUAL(m3::DotProduct(p3, p2), 10, ());
  TEST_EQUAL(m3::DotProduct(p1, p3), 10, ());
}

UNIT_TEST(Point3d_CrossProduct_1)
{
  m3::Point<int> p1(1, 0, 0);
  m3::Point<int> p2(0, 1, 0);
  m3::Point<int> p3(0, 0, 1);

  TEST_EQUAL(m3::CrossProduct(p1, p2), p3, ());
  TEST_EQUAL(m3::CrossProduct(p2, p3), p1, ());
  TEST_EQUAL(m3::CrossProduct(p3, p1), p2, ());
}

UNIT_TEST(Point3d_CrossProduct_2)
{
  m3::Point<int> p1(1, 2, 3);
  m3::Point<int> p2(4, 5, 6);

  TEST_EQUAL(m3::CrossProduct(p1, p2), m3::Point<int>(-3, 6, -3), ());
}

UNIT_TEST(Point3d_CrossProduct_3)
{
  m3::Point<int> p1(3, 7, 1);
  m3::Point<int> p2(6, 2, 9);

  TEST_EQUAL(m3::CrossProduct(p1, p2), m3::Point<int>(61, -21, -36), ());
}

UNIT_TEST(Point3d_RotateX_1)
{
  m3::PointD p(0.0, 1.0, 0.0);
  auto const rotated = p.RotateAroundX(90.0);
  TEST_ALMOST_EQUAL_ABS(rotated, m3::PointD(0.0, 0.0, 1.0), 1e-10, ());
}

UNIT_TEST(Point3d_RotateX_2)
{
  m3::PointD p(1.0, 2.0, 3.0);
  auto const rotated = p.RotateAroundX(90.0);
  TEST_ALMOST_EQUAL_ABS(rotated, m3::PointD(1.0, -3.0, 2.0), 1e-10, ());
}

UNIT_TEST(Point3d_RotateY_1)
{
  m3::PointD p(1.0, 0.0, 0.0);
  auto const rotated = p.RotateAroundY(90.0);
  TEST_ALMOST_EQUAL_ABS(rotated, m3::PointD(0.0, 0.0, -1.0), 1e-10, ());
}

UNIT_TEST(Point3d_RotateY_2)
{
  m3::PointD p(1.0, 2.0, 3.0);
  auto const rotated = p.RotateAroundY(90.0);
  TEST_ALMOST_EQUAL_ABS(rotated, m3::PointD(3.0, 2.0, -1.0), 1e-10, ());
}

UNIT_TEST(Point3d_RotateZ_1)
{
  m3::PointD p(1.0, 0.0, 0.0);
  auto const rotated = p.RotateAroundZ(90.0);
  TEST_ALMOST_EQUAL_ABS(rotated, m3::PointD(0.0, 1.0, 0.0), 1e-10, ());
}

UNIT_TEST(Point3d_RotateZ_2)
{
  m3::PointD p(1.0, 2.0, 3.0);
  auto const rotated = p.RotateAroundZ(90.0);
  TEST_ALMOST_EQUAL_ABS(rotated, m3::PointD(-2.0, 1.0, 3.0), 1e-10, ());
}

UNIT_TEST(Point3d_RotateXYZ)
{
  m3::PointD p(1.0, 1.0, 1.0);
  auto const rotatedFirst = p.RotateAroundZ(-45.0);

  TEST_ALMOST_EQUAL_ABS(rotatedFirst, m3::PointD(std::sqrt(2.0), 0.0, 1.0), 1e-10, ());

  double const angleDegree = math::RadToDeg(acos(rotatedFirst.z / rotatedFirst.Length()));

  auto const north = rotatedFirst.RotateAroundY(-angleDegree);
  TEST_ALMOST_EQUAL_ABS(north, m3::PointD(0.0, 0.0, std::sqrt(3.0)), 1e-10, ());
}
}  // namespace point3d_tests