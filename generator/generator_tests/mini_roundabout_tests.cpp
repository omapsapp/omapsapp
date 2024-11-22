#include "testing/testing.hpp"

#include "generator/mini_roundabout_transformer.hpp"
#include "generator/osm_element.hpp"

#include "coding/point_coding.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/math.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace mini_roundabout_tests
{
using namespace generator;

OsmElement MiniRoundabout(uint64_t id, double lat, double lon)
{
  OsmElement miniRoundabout;
  miniRoundabout.m_id = id;
  miniRoundabout.m_lat = lat;
  miniRoundabout.m_lon = lon;
  miniRoundabout.m_type = OsmElement::EntityType::Node;
  miniRoundabout.AddTag("highway", "mini_roundabout");
  return miniRoundabout;
}

OsmElement Road(uint64_t id, std::vector<uint64_t> && nodes)
{
  OsmElement road;
  road.m_id = id;
  road.m_type = OsmElement::EntityType::Way;
  road.AddTag("highway", "trunk");
  road.m_nodes = nodes;
  return road;
}

OsmElement RoadNode(uint64_t id, double lat, double lon)
{
  OsmElement node;
  node.m_id = id;
  node.m_lat = lat;
  node.m_lon = lon;
  return node;
}

void TestRunCmpPoints(std::vector<m2::PointD> const & pointsFact,
                      std::vector<m2::PointD> const & pointsPlan, double r)
{
  TEST_EQUAL(pointsFact.size(), pointsPlan.size(), ());
  TEST_GREATER(pointsFact.size(), 2, ());
  for (size_t i = 0; i < pointsFact.size(); ++i)
    TEST(AlmostEqualAbs(pointsFact[i], pointsPlan[i], kMwmPointAccuracy), ());
}

void TestRunCmpNumbers(double val1, double val2)
{
  TEST(AlmostEqualAbs(val1, val2, kMwmPointAccuracy), ());
}

UNIT_TEST(PointToPolygon_GeneralProperties)
{
  m2::PointD constexpr center(0.0, 0.0);
  double constexpr r = 10.0;

  std::array<double, 4> const anglesDeg{0.0, -30.0, 30.0, 45.0};

  for (double const & angleDeg : anglesDeg)
  {
    for (size_t verticesCount = 3; verticesCount < 30; ++verticesCount)
    {
      std::vector<m2::PointD> const circlePlain =
          PointToPolygon(center, r, verticesCount, angleDeg);
      double const vertexLenght = DistanceOnPlain(circlePlain.front(), circlePlain.back());

      for (size_t i = 0; i < circlePlain.size() - 1; ++i)
      {
        double const rCurrent = DistanceOnPlain(circlePlain[i], center);
        TEST(AlmostEqualAbs(rCurrent, r, kMwmPointAccuracy), ());

        double const vertexLengthCurrent = DistanceOnPlain(circlePlain[i], circlePlain[i + 1]);
        TEST(AlmostEqualAbs(vertexLengthCurrent, vertexLenght, kMwmPointAccuracy), ());
      }
    }
  }
}

UNIT_TEST(TrimSegment_Vertical)
{
  m2::PointD const a(2.0, -1.0);
  m2::PointD const b(2.0, 3.0);
  double const dist = 1.0;
  m2::PointD const point = GetPointAtDistFromTarget(a /* source */, b /* target */, dist);
  m2::PointD const pointPlan(2.0, 2.0);
  TEST(AlmostEqualAbs(point, pointPlan, kMwmPointAccuracy), ());
}

UNIT_TEST(TrimSegment_VerticalNegative)
{
  m2::PointD const a(-3.0, -5.0);
  m2::PointD const b(-3.0, 6.0);
  double const dist = 4.0;
  m2::PointD const point = GetPointAtDistFromTarget(a /* source */, b /* target */, dist);
  m2::PointD const pointPlan(-3.0, 2.0);
  TEST(AlmostEqualAbs(point, pointPlan, kMwmPointAccuracy), ());
}

UNIT_TEST(TrimSegment_ExceptionalCase)
{
  m2::PointD const a(1.0, 2.0);
  m2::PointD const b(2.0, 3.0);
  double const dist = 10.0;
  m2::PointD const point = GetPointAtDistFromTarget(a /* source */, b /* target */, dist);
  TEST(AlmostEqualAbs(point, a, kMwmPointAccuracy), ());
}

UNIT_TEST(PointToCircle_ZeroMeridian)
{
  ms::LatLon const pointOnZeroMeridian(51.0, 0.0);
  m2::PointD const center = mercator::FromLatLon(pointOnZeroMeridian);
  double const r = mercator::MetersToMercator(100.0);
  auto const circlePlain = PointToPolygon(center, r, 6, 30.0);

  std::vector<m2::PointD> const circlePlainExpected{{0.00077, 59.48054},  {0.00000, 59.48100},
                                                    {-0.00077, 59.48054}, {-0.00077, 59.47964},
                                                    {0.00000, 59.47920},  {0.00077, 59.47964}};

  TestRunCmpPoints(circlePlain, circlePlainExpected, r);
}

UNIT_TEST(PointToCircle_LargeRadius)
{
  ms::LatLon const pointOnZeroMeridian(74.0, 0.1);
  m2::PointD const center = mercator::FromLatLon(pointOnZeroMeridian);
  double const r = mercator::MetersToMercator(500000.0);
  auto const circlePlain = PointToPolygon(center, r, 6, 30.0);

  std::vector<m2::PointD> const circlePlainExpected{{3.99631, 114.67859},  {0.10000, 116.92812},
                                                    {-3.79631, 114.67859}, {-3.79631, 110.17951},
                                                    {0.10000, 107.92998},  {3.99631, 110.17951}};

  TestRunCmpPoints(circlePlain, circlePlainExpected, r);
}

UNIT_TEST(PointToCircle_Equator)
{
  ms::LatLon const pointOnZeroMeridian(0.0, 31.8);
  m2::PointD const center = mercator::FromLatLon(pointOnZeroMeridian);
  double const r = mercator::MetersToMercator(15.0);
  auto const circlePlain = PointToPolygon(center, r, 6, 30.0);

  std::vector<m2::PointD> const circlePlainExpected{
      {31.80011, 0.00006},  {31.80000, 0.00013},  {31.79988, 0.00006},
      {31.79988, -0.00006}, {31.80000, -0.00013}, {31.80011, -0.00006},
  };

  TestRunCmpPoints(circlePlain, circlePlainExpected, r);
}

UNIT_TEST(TrimSegment_Radius3)
{
  m2::PointD const pointOnRoad(10.0, 10.0);
  m2::PointD const pointRoundabout(15.0, 17.0);
  double const r = 3.0;

  m2::PointD const nextPointOnRoad = GetPointAtDistFromTarget(
      pointOnRoad /* source */, pointRoundabout /* target */, r /* dist */);
  double const dist = DistanceOnPlain(nextPointOnRoad, pointRoundabout);
  TestRunCmpNumbers(dist, r);
}

// https://www.openstreetmap.org/node/4999694780
// Simplified example: only one road "Søren R Thornæs veg".
// This road is extended further to Way=511028249
UNIT_TEST(Manage_MiniRoundabout_1Road)
{
  auto const node1 = RoadNode(1, 64.46649, 11.50000);
  auto const node2 = MiniRoundabout(2, 64.46631, 11.50012);
  auto const node3 = RoadNode(3, 64.46620, 11.50016);
  auto const road = Road(100, {node1.m_id, node2.m_id, node3.m_id});

  m2::PointD const center = mercator::FromLatLon({node2.m_lat, node2.m_lon});
  m2::PointD const nearest = mercator::FromLatLon({node1.m_lat, node1.m_lon});

  double const r = mercator::MetersToMercator(2.5);
  auto circlePlain = PointToPolygon(center, r, 6, 30.0);

  // Check for "diameters" equality.
  double const diameter = r * 2.;
  TEST(AlmostEqualAbs(DistanceOnPlain(circlePlain[0], circlePlain[3]), diameter,
                            kMwmPointAccuracy),
       ());
  TEST(AlmostEqualAbs(DistanceOnPlain(circlePlain[1], circlePlain[4]), diameter,
                            kMwmPointAccuracy),
       ());
  TEST(AlmostEqualAbs(DistanceOnPlain(circlePlain[2], circlePlain[5]), diameter,
                            kMwmPointAccuracy),
       ());

  double const edgeLen = DistanceOnPlain(circlePlain[0], circlePlain[1]);
  for (size_t i = 1; i < circlePlain.size(); ++i)
    TEST(AlmostEqualAbs(DistanceOnPlain(circlePlain[i - 1], circlePlain[i]), edgeLen,
                              kMwmPointAccuracy),
         ());

  m2::PointD const newPointOnRoad =
      GetPointAtDistFromTarget(nearest /* source */, center /* target */, r /* dist */);
  AddPointToCircle(circlePlain, newPointOnRoad);

  std::vector<m2::PointD> const circlePlainExpected{
      {11.50013, 85.06309}, {11.50012, 85.06310}, {11.50011, 85.06310}, {11.50010, 85.06309},
      {11.50010, 85.06306}, {11.50012, 85.06305}, {11.50013, 85.06306}};

  TestRunCmpPoints(circlePlain, circlePlainExpected, r);
}

// https://www.openstreetmap.org/node/1617329231
// Mini-roundabout as a part of 4 roads
// Hemel Hempstead magic roundabout
UNIT_TEST(Manage_MiniRoundabout_4Roads)
{
  auto const miniRoundabout = MiniRoundabout(1, 51.7460187, -0.4738389);
  auto const stationRoadNode = RoadNode(2, 51.7459314, -0.4739951);
  auto const plaughRoundaboutNode = RoadNode(3, 51.7461249, -0.4738877);
  auto const stationRoadLeftNode = RoadNode(4, 51.7460327, -0.4739356);  // End of road
  auto const plaughRoundaboutRightNode = RoadNode(5, 51.7458321, -0.4732662);

  m2::PointD const center =
      mercator::FromLatLon({miniRoundabout.m_lat, miniRoundabout.m_lon});

  double const r = mercator::MetersToMercator(2.5);

  auto circlePlain = PointToPolygon(center, r, 6, 30.0);

  AddPointToCircle(circlePlain, GetPointAtDistFromTarget(
                                    mercator::FromLatLon(stationRoadNode.m_lat,
                                                         stationRoadNode.m_lon) /* source */,
                                    center /* target */, r /* dist */));
  AddPointToCircle(circlePlain, GetPointAtDistFromTarget(
                                    mercator::FromLatLon(plaughRoundaboutNode.m_lat,
                                                         plaughRoundaboutNode.m_lon) /* source */,
                                    center /* target */, r /* dist */));
  AddPointToCircle(circlePlain, GetPointAtDistFromTarget(
                                    mercator::FromLatLon(stationRoadLeftNode.m_lat,
                                                         stationRoadLeftNode.m_lon) /* source */,
                                    center /* target */, r /* dist */));
  AddPointToCircle(circlePlain,
                   GetPointAtDistFromTarget(mercator::FromLatLon(plaughRoundaboutRightNode.m_lat,
                                                                 plaughRoundaboutRightNode.m_lon),
                                            center, r));

  std::vector<m2::PointD> const circlePlainExpected{
      {-0.47381, 60.67520}, {-0.47383, 60.67521}, {-0.47384, 60.67521}, {-0.47385, 60.67520},
      {-0.47386, 60.67520}, {-0.47385, 60.67518}, {-0.47385, 60.67518}, {-0.47383, 60.67517},
      {-0.47381, 60.67518}, {-0.47381, 60.67518}};
  TestRunCmpPoints(circlePlain, circlePlainExpected, r);
}

UNIT_TEST(Manage_MiniRoundabout_EqualPoints)
{
  auto const miniRoundabout = MiniRoundabout(1, 10.0, 10.0);

  m2::PointD const center = mercator::FromLatLon({miniRoundabout.m_lat, miniRoundabout.m_lon});

  double const r = mercator::MetersToMercator(5.0);

  auto circlePlain = PointToPolygon(center, r, 12, 30.0);
  AddPointToCircle(circlePlain, circlePlain[11]);
  AddPointToCircle(circlePlain, circlePlain[6]);
  AddPointToCircle(circlePlain, circlePlain[0]);
  AddPointToCircle(circlePlain, circlePlain[0]);
  TEST_EQUAL(circlePlain.size(), 16, ());
}
}  // namespace mini_roundabout_tests
