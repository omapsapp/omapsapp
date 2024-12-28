#include "testing/testing.hpp"

#include "map/elevation_info.hpp"

#include "geometry/point_with_altitude.hpp"
#include "geometry/mercator.hpp"

#include "kml/types.hpp"

namespace geometry
{
using namespace geometry;

UNIT_TEST(ElevationInfo_EmptyMultiGeometry)
{
  ElevationInfo ei;
  TEST_EQUAL(0, ei.GetSize(), ());
  TEST_EQUAL(0, ei.GetAscent(), ());
  TEST_EQUAL(0, ei.GetDescent(), ());
  TEST_EQUAL(ei.GetMinAltitude(), kDefaultAltitudeMeters, ());
  TEST_EQUAL(ei.GetMaxAltitude(), kDefaultAltitudeMeters, ());
}

UNIT_TEST(ElevationInfo_FromMultiGeometry)
{
  kml::MultiGeometry geometry;
  auto const point1 = PointWithAltitude({0.0, 0.0}, 100);
  auto const point2 = PointWithAltitude({1.0, 1.0}, 150);
  auto const point3 = PointWithAltitude({2.0, 2.0}, 50);
  geometry.AddLine({
    point1,
    point2,
    point3
  });
  ElevationInfo ei(geometry);

  TEST_EQUAL(3, ei.GetSize(), ());
  TEST_EQUAL(ei.GetMinAltitude(), 50, ());
  TEST_EQUAL(ei.GetMaxAltitude(), 150, ());
  TEST_EQUAL(ei.GetAscent(), 50, ()); // Ascent from 100 -> 150
  TEST_EQUAL(ei.GetDescent(), 100, ()); // Descent from 150 -> 50

  double distance = 0;
  TEST_EQUAL(ei.GetPoints()[0].m_distance, distance, ());
  distance += mercator::DistanceOnEarth(point1, point2);
  TEST_EQUAL(ei.GetPoints()[1].m_distance, distance, ());
  distance += mercator::DistanceOnEarth(point2, point3);
  TEST_EQUAL(ei.GetPoints()[2].m_distance, distance, ());
}

UNIT_TEST(ElevationInfo_NoAltitudePoints)
{
  kml::MultiGeometry geometry;
  geometry.AddLine({
    PointWithAltitude({0.0, 0.0}),
    PointWithAltitude({1.0, 1.0}),
    PointWithAltitude({2.0, 2.0})
  });
  ElevationInfo ei(geometry);

  TEST_EQUAL(3, ei.GetSize(), ());
  TEST_EQUAL(ei.GetMinAltitude(), kDefaultAltitudeMeters, ());
  TEST_EQUAL(ei.GetMaxAltitude(), kDefaultAltitudeMeters, ());
  TEST_EQUAL(ei.GetAscent(), 0, ());
  TEST_EQUAL(ei.GetDescent(), 0, ());
}

UNIT_TEST(ElevationInfo_MultipleLines)
{
  kml::MultiGeometry geometry;
  geometry.AddLine({
    PointWithAltitude({0.0, 0.0}, 100),
    PointWithAltitude({1.0, 1.0}, 150),
    PointWithAltitude({1.0, 1.0}, 140)
  });
  geometry.AddLine({
    PointWithAltitude({2.0, 2.0}, 50),
    PointWithAltitude({3.0, 3.0}, 75),
    PointWithAltitude({3.0, 3.0}, 60)
  });
  geometry.AddLine({
    PointWithAltitude({4.0, 4.0}, 200),
    PointWithAltitude({5.0, 5.0}, 250)
  });
  ElevationInfo ei(geometry);

  TEST_EQUAL(8, ei.GetSize(), ());
  TEST_EQUAL(ei.GetMinAltitude(), 50, ());
  TEST_EQUAL(ei.GetMaxAltitude(), 250, ());
  TEST_EQUAL(ei.GetAscent(), 125, ()); // Ascent from 100 -> 150, 50 -> 75, 200 -> 250
  TEST_EQUAL(ei.GetDescent(), 25, ()); // Descent from 150 -> 140, 75 -> 60
}

UNIT_TEST(ElevationInfo_SegmentDistances)
{
  kml::MultiGeometry geometry;
  geometry.AddLine({
    geometry::PointWithAltitude({0.0, 0.0}),
    geometry::PointWithAltitude({1.0, 0.0})
  });
  geometry.AddLine({
    geometry::PointWithAltitude({2.0, 0.0}),
    geometry::PointWithAltitude({3.0, 0.0})
  });
  geometry.AddLine({
    geometry::PointWithAltitude({4.0, 0.0}),
    geometry::PointWithAltitude({5.0, 0.0})
  });

  ElevationInfo ei(geometry);
  auto const & segmentDistances = ei.GetSegmentsDistances();
  auto const points = ei.GetPoints();

  TEST_EQUAL(segmentDistances.size(), 2, ());
  TEST_EQUAL(segmentDistances[0], ei.GetPoints()[2].m_distance, ());
  TEST_EQUAL(segmentDistances[1], ei.GetPoints()[4].m_distance, ());
}

UNIT_TEST(ElevationInfo_PositiveAndNegativeAltitudes)
{
  kml::MultiGeometry geometry;
  geometry.AddLine({
    PointWithAltitude({0.0, 0.0}, -10),
    PointWithAltitude({1.0, 1.0}, 20),
    PointWithAltitude({2.0, 2.0}, -5),
    PointWithAltitude({3.0, 3.0}, 15)
  });
  ElevationInfo ei(geometry);

  TEST_EQUAL(4, ei.GetSize(), ());
  TEST_EQUAL(ei.GetMinAltitude(), -10, ());
  TEST_EQUAL(ei.GetMaxAltitude(), 20, ());
  TEST_EQUAL(ei.GetAscent(), 50, ()); // Ascent from -10 -> 20 and -5 -> 15
  TEST_EQUAL(ei.GetDescent(), 25, ()); // Descent from 20 -> -5
}
} // namespace geometry
