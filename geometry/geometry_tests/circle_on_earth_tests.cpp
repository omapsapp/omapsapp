#include "testing/testing.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/point2d.hpp"
#include "geometry/mercator.hpp"
#include "geometry/circle_on_earth.hpp"

#include "base/math.hpp"

#include <sstream>
#include <vector>

namespace
{
void TestGeometryAlmostEqualAbs(std::vector<m2::PointD> const & geometry,
                                std::vector<m2::PointD> const & answer,
                                double eps)
{
  TEST_EQUAL(geometry.size(), answer.size(), ());

  for (size_t i = 0; i < geometry.size(); ++i)
    TEST_ALMOST_EQUAL_ABS(geometry[i], answer[i], eps, ());
}

void TestGeometryAlmostEqualMeters(std::vector<m2::PointD> const & geometry,
                                      std::vector<m2::PointD> const & answer,
                                      double epsMeters)
{
  TEST_EQUAL(geometry.size(), answer.size(), ());

  for (size_t i = 0; i < geometry.size(); ++i)
    TEST_LESS(mercator::DistanceOnEarth(geometry[i], answer[i]), epsMeters, ());
}
}  // namespace

UNIT_TEST(CircleOnEarth)
{
  ms::LatLon const center(90.0 /* lat */, 0.0 /* lon */);
  double const radiusMeters = 2.0 * math::pi * ms::kEarthRadiusMeters / 4.0;
  auto const geometry =
      ms::CreateCircleGeometryOnEarth(center, radiusMeters, 90.0 /* angleStepDegree */);

  std::vector<m2::PointD> const result = {
      mercator::FromLatLon(0.0, 0.0),
      mercator::FromLatLon(0.0, 90.0),
      mercator::FromLatLon(0.0, 180.0),
      mercator::FromLatLon(0.0, -90.0)
  };

  TestGeometryAlmostEqualAbs(geometry, result, 1e-9 /* eps */);
}

UNIT_TEST(CircleOnEarthEquator)
{
  ms::LatLon const center(0.0 /* lat */, 0.0 /* lon */);
  auto const point = mercator::FromLatLon(center);

  auto constexpr kRadiusMeters = 10000.0;
  double const kRadiusMercator = mercator::MetersToMercator(kRadiusMeters);
  auto constexpr kAngleStepDegree = 30.0;
  auto constexpr kN = static_cast<size_t>(360.0 / kAngleStepDegree);

  std::vector<m2::PointD> result;
  result.reserve(kN);
  auto constexpr kStepRad = math::DegToRad(kAngleStepDegree);
  double angleSumRad = 0.0;
  double angleRad = -math::pi2;
  while (angleSumRad < 2 * math::pi)
  {
    angleSumRad += kStepRad;
    result.emplace_back(point.x + kRadiusMercator * cos(angleRad),
                        point.y + kRadiusMercator * sin(angleRad));
    angleRad += kStepRad;
    if (angleRad > 2 * math::pi)
      angleRad -= 2 * math::pi;
  }

  auto const geometry =
      ms::CreateCircleGeometryOnEarth(center, kRadiusMeters, kAngleStepDegree);

  TestGeometryAlmostEqualMeters(geometry, result, 20.0 /* epsMeters */);
}
