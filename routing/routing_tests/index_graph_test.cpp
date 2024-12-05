#include "testing/testing.hpp"

#include "routing/routing_tests/index_graph_tools.hpp"

#include "routing/base/astar_algorithm.hpp"
#include "routing/base/astar_graph.hpp"

#include "routing/edge_estimator.hpp"
#include "routing/fake_ending.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_serialization.hpp"
#include "routing/index_graph_starter.hpp"
#include "routing/index_router.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/vehicle_mask.hpp"

#include "routing_common/car_model.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include "coding/reader.hpp"
#include "coding/writer.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace index_graph_test
{
using namespace routing;
using namespace routing_test;
using namespace std;

using measurement_utils::KmphToMps;

using TestEdge = TestIndexGraphTopology::Edge;

using AlgorithmForIndexGraphStarter = AStarAlgorithm<Segment, SegmentEdge, RouteWeight>;

double constexpr kUnknownWeight = -1.0;

void TestRoute(FakeEnding const & start, FakeEnding const & finish, size_t expectedLength,
               vector<Segment> const * expectedRoute, double expectedWeight, WorldGraph & graph)
{
  auto starter = MakeStarter(start, finish, graph);
  vector<Segment> route;
  double timeSec;
  auto const resultCode = CalculateRoute(*starter, route, timeSec);
  TEST_EQUAL(resultCode, AlgorithmForIndexGraphStarter::Result::OK, ());

  TEST_GREATER(route.size(), 2, ());

  vector<Segment> noFakeRoute;
  for (auto const & s : route)
  {
    auto real = s;
    if (!starter->ConvertToReal(real))
      continue;
    noFakeRoute.push_back(real);
  }

  TEST_EQUAL(noFakeRoute.size(), expectedLength, ("route =", noFakeRoute));

  if (expectedWeight != kUnknownWeight)
  {
    double constexpr kEpsilon = 0.01;
    TEST(AlmostEqualRel(timeSec, expectedWeight, kEpsilon),
         ("Expected weight:", expectedWeight, "got:", timeSec));
  }

  if (expectedRoute)
    TEST_EQUAL(noFakeRoute, *expectedRoute, ());
}

void TestEdges(IndexGraph & graph, Segment const & segment, vector<Segment> const & expectedTargets,
               bool isOutgoing)
{
  ASSERT(segment.IsForward() || !graph.GetRoadGeometry(segment.GetFeatureId()).IsOneWay(), ());

  IndexGraph::SegmentEdgeListT edges;
  graph.GetEdgeList(segment, isOutgoing, true /* useRoutingOptions */, edges);

  vector<Segment> targets;
  for (SegmentEdge const & edge : edges)
    targets.push_back(edge.GetTarget());

  sort(targets.begin(), targets.end());

  vector<Segment> sortedExpectedTargets(expectedTargets);
  sort(sortedExpectedTargets.begin(), sortedExpectedTargets.end());

  TEST_EQUAL(targets, sortedExpectedTargets, ());
}

void TestOutgoingEdges(IndexGraph & graph, Segment const & segment,
                       vector<Segment> const & expectedTargets)
{
  TestEdges(graph, segment, expectedTargets, true /* isOutgoing */);
}

void TestIngoingEdges(IndexGraph & graph, Segment const & segment,
                      vector<Segment> const & expectedTargets)
{
  TestEdges(graph, segment, expectedTargets, false /* isOutgoing */);
}

uint32_t AbsDelta(uint32_t v0, uint32_t v1) { return v0 > v1 ? v0 - v1 : v1 - v0; }

//                   R4 (one way down)
//
// R1     J2--------J3         -1
//         ^         v
//         ^         v
// R0 *---J0----*---J1----*     0
//         ^         v
//         ^         v
// R2     J4--------J5          1
//
//        R3 (one way up)       y
//
// x: 0    1    2    3    4
//
UNIT_TEST(EdgesTest)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(
      0 /* featureId */, false, 1.0 /* speed */,
      RoadGeometry::Points({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {3.0, 0.0}, {4.0, 0.0}}));
  loader->AddRoad(1 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, -1.0}, {3.0, -1.0}}));
  loader->AddRoad(2 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, -1.0}, {3.0, -1.0}}));
  loader->AddRoad(3 /* featureId */, true, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 1.0}, {1.0, 0.0}, {1.0, -1.0}}));
  loader->AddRoad(4 /* featureId */, true, 1.0 /* speed */,
                  RoadGeometry::Points({{3.0, -1.0}, {3.0, 0.0}, {3.0, 1.0}}));

  traffic::TrafficCache const trafficCache;
  IndexGraph graph(make_shared<Geometry>(std::move(loader)), CreateEstimatorForCar(trafficCache));

  vector<Joint> joints;
  joints.emplace_back(MakeJoint({{0, 1}, {3, 1}}));  // J0
  joints.emplace_back(MakeJoint({{0, 3}, {4, 1}}));  // J1
  joints.emplace_back(MakeJoint({{1, 0}, {3, 2}}));  // J2
  joints.emplace_back(MakeJoint({{1, 1}, {4, 0}}));  // J3
  joints.emplace_back(MakeJoint({{2, 0}, {3, 0}}));  // J4
  joints.emplace_back(MakeJoint({{2, 1}, {4, 2}}));  // J5
  graph.Import(joints);

  TestOutgoingEdges(
      graph, {kTestNumMwmId, 0 /* featureId */, 0 /* segmentIdx */, true /* forward */},
      {{kTestNumMwmId, 0, 0, false}, {kTestNumMwmId, 0, 1, true}, {kTestNumMwmId, 3, 1, true}});
  TestIngoingEdges(graph, {kTestNumMwmId, 0, 0, true}, {{kTestNumMwmId, 0, 0, false}});
  TestOutgoingEdges(graph, {kTestNumMwmId, 0, 0, false}, {{kTestNumMwmId, 0, 0, true}});
  TestIngoingEdges(
      graph, {kTestNumMwmId, 0, 0, false},
      {{kTestNumMwmId, 0, 0, true}, {kTestNumMwmId, 0, 1, false}, {kTestNumMwmId, 3, 0, true}});

  TestOutgoingEdges(
      graph, {kTestNumMwmId, 0, 2, true},
      {{kTestNumMwmId, 0, 2, false}, {kTestNumMwmId, 0, 3, true}, {kTestNumMwmId, 4, 1, true}});
  TestIngoingEdges(graph, {kTestNumMwmId, 0, 2, true}, {{kTestNumMwmId, 0, 1, true}});
  TestOutgoingEdges(graph, {kTestNumMwmId, 0, 2, false}, {{kTestNumMwmId, 0, 1, false}});
  TestIngoingEdges(
      graph, {kTestNumMwmId, 0, 2, false},
      {{kTestNumMwmId, 0, 2, true}, {kTestNumMwmId, 0, 3, false}, {kTestNumMwmId, 4, 0, true}});

  TestOutgoingEdges(
      graph, {kTestNumMwmId, 3, 0, true},
      {{kTestNumMwmId, 3, 1, true}, {kTestNumMwmId, 0, 0, false}, {kTestNumMwmId, 0, 1, true}});
  TestIngoingEdges(graph, {kTestNumMwmId, 3, 0, true}, {{kTestNumMwmId, 2, 0, false}});

  TestOutgoingEdges(graph, {kTestNumMwmId, 4, 1, true}, {{kTestNumMwmId, 2, 0, false}});
  TestIngoingEdges(
      graph, {kTestNumMwmId, 4, 1, true},
      {{kTestNumMwmId, 4, 0, true}, {kTestNumMwmId, 0, 2, true}, {kTestNumMwmId, 0, 3, false}});
}

//  Roads     R1:
//
//            -2
//            -1
//  R0  -2 -1  0  1  2
//             1
//             2
//
UNIT_TEST(FindPathCross)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(
      0 /* featureId */, false, 1.0 /* speed */,
      RoadGeometry::Points({{-2.0, 0.0}, {-1.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}}));
  loader->AddRoad(
      1 /* featureId */, false, 1.0 /* speed */,
      RoadGeometry::Points({{0.0, -2.0}, {0.0, -1.0}, {0.0, 0.0}, {0.0, 1.0}, {0.0, 2.0}}));

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);
  unique_ptr<WorldGraph> worldGraph =
      BuildWorldGraph(std::move(loader), estimator, {MakeJoint({{0, 2}, {1, 2}})});

  vector<FakeEnding> endPoints;
  for (uint32_t i = 0; i < 4; ++i)
  {
    endPoints.push_back(MakeFakeEnding(0 /* featureId */, i /* segmentIdx */,
                                       m2::PointD(-1.5 + i, 0.0), *worldGraph));
    endPoints.push_back(MakeFakeEnding(1, i, m2::PointD(0.0, -1.5 + i), *worldGraph));
  }

  for (auto const & start : endPoints)
  {
    for (auto const & finish : endPoints)
    {
      uint32_t expectedLength = 0;
      if (start.m_projections[0].m_segment.GetFeatureId() ==
          finish.m_projections[0].m_segment.GetFeatureId())
      {
        expectedLength = AbsDelta(start.m_projections[0].m_segment.GetSegmentIdx(),
                                  finish.m_projections[0].m_segment.GetSegmentIdx()) +
                         1;
      }
      else
      {
        if (start.m_projections[0].m_segment.GetSegmentIdx() < 2)
          expectedLength += 2 - start.m_projections[0].m_segment.GetSegmentIdx();
        else
          expectedLength += start.m_projections[0].m_segment.GetSegmentIdx() - 1;

        if (finish.m_projections[0].m_segment.GetSegmentIdx() < 2)
          expectedLength += 2 - finish.m_projections[0].m_segment.GetSegmentIdx();
        else
          expectedLength += finish.m_projections[0].m_segment.GetSegmentIdx() - 1;
      }
      TestRoute(start, finish, expectedLength, nullptr /* expectedRoute */, kUnknownWeight,
                *worldGraph);
    }
  }
}

// Roads   R4  R5  R6  R7
//
//    R0   0 - * - * - *
//         |   |   |   |
//    R1   * - 1 - * - *
//         |   |   |   |
//    R2   * - * - 2 - *
//         |   |   |   |
//    R3   * - * - * - 3
//
UNIT_TEST(FindPathManhattan)
{
  uint32_t constexpr kCitySize = 4;
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  for (uint32_t i = 0; i < kCitySize; ++i)
  {
    RoadGeometry::Points street;
    RoadGeometry::Points avenue;
    for (uint32_t j = 0; j < kCitySize; ++j)
    {
      street.emplace_back(static_cast<double>(j), static_cast<double>(i));
      avenue.emplace_back(static_cast<double>(i), static_cast<double>(j));
    }
    loader->AddRoad(i, false, 1.0 /* speed */, street);
    loader->AddRoad(i + kCitySize, false, 1.0 /* speed */, avenue);
  }

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);

  vector<Joint> joints;
  for (uint32_t i = 0; i < kCitySize; ++i)
  {
    for (uint32_t j = 0; j < kCitySize; ++j)
      joints.emplace_back(MakeJoint({{i, j}, {j + kCitySize, i}}));
  }

  unique_ptr<WorldGraph> worldGraph = BuildWorldGraph(std::move(loader), estimator, joints);

  vector<FakeEnding> endPoints;
  for (uint32_t featureId = 0; featureId < kCitySize; ++featureId)
  {
    for (uint32_t segmentId = 0; segmentId < kCitySize - 1; ++segmentId)
    {
      endPoints.push_back(MakeFakeEnding(featureId, segmentId,
                                         m2::PointD(0.5 + segmentId, featureId), *worldGraph));
      endPoints.push_back(MakeFakeEnding(featureId + kCitySize, segmentId,
                                         m2::PointD(featureId, 0.5 + segmentId), *worldGraph));
    }
  }

  for (auto const & start : endPoints)
  {
    for (auto const & finish : endPoints)
    {
      uint32_t expectedLength = 0;

      auto const startFeatureOffset =
          start.m_projections[0].m_segment.GetFeatureId() < kCitySize
              ? start.m_projections[0].m_segment.GetFeatureId()
              : start.m_projections[0].m_segment.GetFeatureId() - kCitySize;
      auto const finishFeatureOffset =
          finish.m_projections[0].m_segment.GetFeatureId() < kCitySize
              ? finish.m_projections[0].m_segment.GetFeatureId()
              : finish.m_projections[0].m_segment.GetFeatureId() - kCitySize;

      if ((start.m_projections[0].m_segment.GetFeatureId() < kCitySize) ==
          (finish.m_projections[0].m_segment.GetFeatureId() < kCitySize))
      {
        uint32_t segDelta = AbsDelta(start.m_projections[0].m_segment.GetSegmentIdx(),
                                     finish.m_projections[0].m_segment.GetSegmentIdx());
        if (segDelta == 0 && start.m_projections[0].m_segment.GetFeatureId() !=
                                 finish.m_projections[0].m_segment.GetFeatureId())
          segDelta = 1;
        expectedLength += segDelta;
        expectedLength += AbsDelta(startFeatureOffset, finishFeatureOffset) + 1;
      }
      else
      {
        if (start.m_projections[0].m_segment.GetSegmentIdx() < finishFeatureOffset)
        {
          expectedLength += finishFeatureOffset - start.m_projections[0].m_segment.GetSegmentIdx();
        }
        else
        {
          expectedLength +=
              start.m_projections[0].m_segment.GetSegmentIdx() - finishFeatureOffset + 1;
        }

        if (finish.m_projections[0].m_segment.GetSegmentIdx() < startFeatureOffset)
        {
          expectedLength += startFeatureOffset - finish.m_projections[0].m_segment.GetSegmentIdx();
        }
        else
        {
          expectedLength +=
              finish.m_projections[0].m_segment.GetSegmentIdx() - startFeatureOffset + 1;
        }
      }

      TestRoute(start, finish, expectedLength, nullptr /* expectedRoute */, kUnknownWeight,
                *worldGraph);
    }
  }
}

// Roads                                          y:
//
//  fast road R0              * - * - *           -1
//                          ╱           ╲
//  slow road R1    * - -  *  - - * - -  * - - *   0
//                        J0            J1
//
//            x:    0      1  2   3   4  5     6
//
UNIT_TEST(RoadSpeed)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();

  loader->AddRoad(
      0 /* featureId */, false, 10.0 /* speed */,
      RoadGeometry::Points({{1.0, 0.0}, {2.0, -1.0}, {3.0, -1.0}, {4.0, -1.0}, {5.0, 0.0}}));

  loader->AddRoad(
      1 /* featureId */, false, 1.0 /* speed */,
      RoadGeometry::Points({{0.0, 0.0}, {1.0, 0.0}, {3.0, 0.0}, {5.0, 0.0}, {6.0, 0.0}}));

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);

  vector<Joint> joints;
  joints.emplace_back(MakeJoint({{0, 0}, {1, 1}}));  // J0
  joints.emplace_back(MakeJoint({{0, 4}, {1, 3}}));  // J1

  unique_ptr<WorldGraph> worldGraph = BuildWorldGraph(std::move(loader), estimator, joints);

  auto const start =
      MakeFakeEnding(1 /* featureId */, 0 /* segmentIdx */, m2::PointD(0.5, 0), *worldGraph);
  auto const finish = MakeFakeEnding(1, 3, m2::PointD(5.5, 0), *worldGraph);

  vector<Segment> const expectedRoute({{kTestNumMwmId, 1, 0, true},
                                       {kTestNumMwmId, 0, 0, true},
                                       {kTestNumMwmId, 0, 1, true},
                                       {kTestNumMwmId, 0, 2, true},
                                       {kTestNumMwmId, 0, 3, true},
                                       {kTestNumMwmId, 1, 3, true}});
  double const expectedWeight =
      mercator::DistanceOnEarth({0.5, 0.0}, {1.0, 0.0}) / KmphToMps(1.0) +
      mercator::DistanceOnEarth({1.0, 0.0}, {2.0, -1.0}) / KmphToMps(10.0) +
      mercator::DistanceOnEarth({2.0, -1.0}, {4.0, -1.0}) / KmphToMps(10.0) +
      mercator::DistanceOnEarth({4.0, -1.0}, {5.0, 0.0}) / KmphToMps(10.0) +
      mercator::DistanceOnEarth({5.0, 0.0}, {5.5, 0.0}) / KmphToMps(1.0);
  TestRoute(start, finish, expectedRoute.size(), &expectedRoute, expectedWeight, *worldGraph);
}

// Roads                             y:
//
//    R0    * - - - - - - - - *      0
//                ^     ^
//             start   finish
//
//    x:    0     1     2     3
//
UNIT_TEST(OneSegmentWay)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();

  loader->AddRoad(0 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {3.0, 0.0}}));

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);
  unique_ptr<WorldGraph> worldGraph = BuildWorldGraph(std::move(loader), estimator, vector<Joint>());
  vector<Segment> const expectedRoute(
      {{kTestNumMwmId, 0 /* featureId */, 0 /* seg id */, true /* forward */}});

  // Starter must match any combination of start and finish directions.
  vector<bool> const tf = {{true, false}};
  for (auto const startIsForward : tf)
  {
    for (auto const finishIsForward : tf)
    {
      auto const start = MakeFakeEnding({Segment(kTestNumMwmId, 0, 0, startIsForward)},
                                        m2::PointD(1.0, 0.0), *worldGraph);
      auto const finish = MakeFakeEnding({Segment(kTestNumMwmId, 0, 0, finishIsForward)},
                                         m2::PointD(2.0, 0.0), *worldGraph);

      auto const expectedWeight = mercator::DistanceOnEarth({1.0, 0.0}, {2.0, 0.0}) / KmphToMps(1.0);
      TestRoute(start, finish, expectedRoute.size(), &expectedRoute, expectedWeight, *worldGraph);
    }
  }
}

//
// Roads                             y:
//
//    R0    * - - - - - - - ->*      0
//                ^     ^
//            finish  start
//
//    x:    0     1     2     3
//
UNIT_TEST(OneSegmentWayBackward)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();

  loader->AddRoad(0 /* featureId */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {3.0, 0.0}}));

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);
  unique_ptr<WorldGraph> worldGraph = BuildWorldGraph(std::move(loader), estimator, vector<Joint>());

  auto const start =
      MakeFakeEnding(0 /* featureId */, 0 /* segmentIdx */, m2::PointD(2, 0), *worldGraph);
  auto const finish = MakeFakeEnding(0, 0, m2::PointD(1, 0), *worldGraph);

  auto starter = MakeStarter(start, finish, *worldGraph);
  vector<Segment> route;
  double timeSec;
  auto const resultCode = CalculateRoute(*starter, route, timeSec);
  TEST_EQUAL(resultCode, AlgorithmForIndexGraphStarter::Result::NoPath, ());
}

// Roads                             y:
//
//    R0  * - - - - - * - - - - - * R1      0
//              ^           ^
//           start        finish
//
//    x:  0     1     2     3     4
//
UNIT_TEST(FakeSegmentCoordinates)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();

  loader->AddRoad(0 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {2.0, 0.0}}));
  loader->AddRoad(1 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {4.0, 0.0}}));

  vector<Joint> joints;
  joints.emplace_back(MakeJoint({{0 /* featureId */, 1 /* pointId */}, {1, 0}}));

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);
  unique_ptr<WorldGraph> worldGraph = BuildWorldGraph(std::move(loader), estimator, joints);
  vector<m2::PointD> const expectedGeom = {{1.0 /* x */, 0.0 /* y */}, {2.0, 0.0}, {3.0, 0.0}};

  // Test fake segments have valid coordinates for any combination of start and finish directions
  vector<bool> const tf = {{true, false}};
  for (auto const startIsForward : tf)
  {
    for (auto const finishIsForward : tf)
    {
      auto const start = MakeFakeEnding({Segment(kTestNumMwmId, 0, 0, startIsForward)},
                                        m2::PointD(1, 0), *worldGraph);
      auto const finish = MakeFakeEnding({Segment(kTestNumMwmId, 1, 0, finishIsForward)},
                                         m2::PointD(3, 0), *worldGraph);

      auto starter = MakeStarter(start, finish, *worldGraph);
      TestRouteGeometry(*starter, AlgorithmForIndexGraphStarter::Result::OK, expectedGeom);
    }
  }
}

//       start  finish
//          *     *
//          |     |                                   R1
//          |     |                                   *
//    * - - - - - - - - - - - - - - - - - - - - - - - *
//                         R0
//
// x: 0     1     2     3     4     5     6     7     8
//
UNIT_TEST(FakeEndingAStarInvariant)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();

  loader->AddRoad(0 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {8.0, 0.0}}));
  loader->AddRoad(1 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{8.0, 0.0}, {8.0, 1.0 / 1000.0}}));

  vector<Joint> joints;
  joints.emplace_back(MakeJoint({{0 /* featureId */, 1 /* pointId */}, {1, 0}}));

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);
  unique_ptr<WorldGraph> worldGraph = BuildWorldGraph(std::move(loader), estimator, joints);
  vector<Segment> const expectedRoute(
      {{kTestNumMwmId, 0 /* featureId */, 0 /* seg id */, true /* forward */}});

  auto const start =
      MakeFakeEnding(0 /* featureId */, 0 /* segmentIdx */, m2::PointD(1.0, 1.0), *worldGraph);
  auto const finish = MakeFakeEnding(0, 0, m2::PointD(2.0, 1.0), *worldGraph);

  auto const expectedWeight =
      estimator->CalcOffroad({1.0, 1.0}, {1.0, 0.0}, EdgeEstimator::Purpose::Weight) +
      mercator::DistanceOnEarth({1.0, 0.0}, {2.0, 0.0}) / KmphToMps(1.0) +
          estimator->CalcOffroad({2.0, 0.0}, {2.0, 1.0}, EdgeEstimator::Purpose::Weight);
  TestRoute(start, finish, expectedRoute.size(), &expectedRoute, expectedWeight, *worldGraph);
}

//
//  Road       R0 (ped)       R1 (car)       R2 (car)
//           0----------1 * 0----------1 * 0----------1
//  Joints               J0             J1
//
UNIT_TEST(SerializeSimpleGraph)
{
  vector<uint8_t> buffer;
  {
    IndexGraph graph;
    vector<Joint> joints = {
        MakeJoint({{0, 1}, {1, 0}}), MakeJoint({{1, 1}, {2, 0}}),
    };
    graph.Import(joints);
    unordered_map<uint32_t, VehicleMask> masks;
    masks[0] = kPedestrianMask;
    masks[1] = kCarMask;
    masks[2] = kCarMask;

    MemWriter<vector<uint8_t>> writer(buffer);
    IndexGraphSerializer::Serialize(graph, masks, writer);
  }

  {
    IndexGraph graph;
    MemReader reader(buffer.data(), buffer.size());
    ReaderSource<MemReader> source(reader);
    IndexGraphSerializer::Deserialize(graph, source, kAllVehiclesMask);

    TEST_EQUAL(graph.GetNumRoads(), 3, ());
    TEST_EQUAL(graph.GetNumJoints(), 2, ());
    TEST_EQUAL(graph.GetNumPoints(), 4, ());

    TEST_EQUAL(graph.GetJointId({0, 0}), Joint::kInvalidId, ());
    TEST_EQUAL(graph.GetJointId({0, 1}), 1, ());
    TEST_EQUAL(graph.GetJointId({1, 0}), 1, ());
    TEST_EQUAL(graph.GetJointId({1, 1}), 0, ());
    TEST_EQUAL(graph.GetJointId({2, 0}), 0, ());
    TEST_EQUAL(graph.GetJointId({2, 1}), Joint::kInvalidId, ());
  }

  {
    IndexGraph graph;
    MemReader reader(buffer.data(), buffer.size());
    ReaderSource<MemReader> source(reader);
    IndexGraphSerializer::Deserialize(graph, source, kCarMask);

    TEST_EQUAL(graph.GetNumRoads(), 2, ());
    TEST_EQUAL(graph.GetNumJoints(), 1, ());
    TEST_EQUAL(graph.GetNumPoints(), 2, ());

    TEST_EQUAL(graph.GetJointId({0, 0}), Joint::kInvalidId, ());
    TEST_EQUAL(graph.GetJointId({0, 1}), Joint::kInvalidId, ());
    TEST_EQUAL(graph.GetJointId({1, 0}), Joint::kInvalidId, ());
    TEST_EQUAL(graph.GetJointId({1, 1}), 0, ());
    TEST_EQUAL(graph.GetJointId({2, 0}), 0, ());
    TEST_EQUAL(graph.GetJointId({2, 1}), Joint::kInvalidId, ());
  }
}

//      Finish
// 0.0004    *
//           ^
//           |
//           F2
//           |
//           |
// 0.0003   6*---------*5
//           |         |
//           |         |
//           |         |
//           |         |
//           |         |
// 0.0002   7*         *4
//           |         |
//           |         |
// 0.00015  8*    F0   *3
//            \       /
//              \   /    1    0
// 0.0001        9*---F0-*----*
//                2           ^
//                            |
//                            F1
//                            |
//                            |
// 0                          * Start
//   0         0.0001       0.0002
// F0 is a two-way feature with a loop and F1 and F2 are an one-way one-segment features.
unique_ptr<SingleVehicleWorldGraph> BuildLoopGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* feature id */, false /* one way */, 100.0 /* speed */,
                  RoadGeometry::Points({{0.0002, 0.0001},
                                        {0.00015, 0.0001},
                                        {0.0001, 0.0001},
                                        {0.00015, 0.00015},
                                        {0.00015, 0.0002},
                                        {0.00015, 0.0003},
                                        {0.00005, 0.0003},
                                        {0.00005, 0.0002},
                                        {0.00005, 0.00015},
                                        {0.0001, 0.0001}}));
  loader->AddRoad(1 /* feature id */, true /* one way */, 100.0 /* speed */,
                  RoadGeometry::Points({{0.0002, 0.0}, {0.0002, 0.0001}}));
  loader->AddRoad(2 /* feature id */, true /* one way */, 100.0 /* speed */,
                  RoadGeometry::Points({{0.00005, 0.0003}, {0.00005, 0.0004}}));

  vector<Joint> const joints = {
      MakeJoint({{0 /* feature id */, 2 /* point id */}, {0, 9}}), /* joint at point (0.0002, 0) */
      MakeJoint({{1, 1}, {0, 0}}), /* joint at point (0.0002, 0.0001) */
      MakeJoint({{0, 6}, {2, 0}}), /* joint at point (0.00005, 0.0003) */
      MakeJoint({{2, 1}}),         /* joint at point (0.00005, 0.0004) */
  };

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);
  return BuildWorldGraph(std::move(loader), estimator, joints);
}

// This test checks that the route from Start to Finish doesn't make an extra loop in F0.
// If it was so the route time had been much more.
UNIT_CLASS_TEST(RestrictionTest, LoopGraph)
{
  Init(BuildLoopGraph());
  auto start =
      MakeFakeEnding(1 /* featureId */, 0 /* segmentIdx */, m2::PointD(0.0002, 0), *m_graph);
  auto finish = MakeFakeEnding(2, 0, m2::PointD(0.00005, 0.0004), *m_graph);

  vector<Segment> const expectedRoute = {{kTestNumMwmId, 1, 0, true},  {kTestNumMwmId, 0, 0, true},
                                         {kTestNumMwmId, 0, 1, true},  {kTestNumMwmId, 0, 8, false},
                                         {kTestNumMwmId, 0, 7, false}, {kTestNumMwmId, 0, 6, false},
                                         {kTestNumMwmId, 2, 0, true}};

  auto const expectedWeight =
      mercator::DistanceOnEarth({0.0002, 0.0}, {0.0002, 0.0001}) / KmphToMps(100.0) +
      mercator::DistanceOnEarth({0.0002, 0.0001}, {0.00015, 0.0001}) / KmphToMps(100.0) +
      mercator::DistanceOnEarth({0.00015, 0.0001}, {0.0001, 0.0001}) / KmphToMps(100.0) +
      mercator::DistanceOnEarth({0.0001, 0.0001}, {0.00005, 0.00015}) / KmphToMps(100.0) +
      mercator::DistanceOnEarth({0.00005, 0.00015}, {0.00005, 0.0002}) / KmphToMps(100.0) +
      mercator::DistanceOnEarth({0.00005, 0.0002}, {0.00005, 0.0003}) / KmphToMps(100.0) +
      mercator::DistanceOnEarth({0.00005, 0.0003}, {0.00005, 0.0004}) / KmphToMps(100.0);
  TestRoute(start, finish, expectedRoute.size(), &expectedRoute, expectedWeight, *m_graph);
}

UNIT_TEST(IndexGraph_OnlyTopology_1)
{
  // Add edges to the graph in the following format: (from, to, weight).

  uint32_t const numVertices = 5;
  TestIndexGraphTopology graph(numVertices);
  graph.AddDirectedEdge(0, 1, 1.0);
  graph.AddDirectedEdge(0, 2, 2.0);
  graph.AddDirectedEdge(1, 3, 1.0);
  graph.AddDirectedEdge(2, 3, 2.0);

  double const expectedWeight = 2.0;
  vector<TestEdge> const expectedEdges = {{0, 1}, {1, 3}};

  TestTopologyGraph(graph, 0, 3, true /* pathFound */, expectedWeight, expectedEdges);
  TestTopologyGraph(graph, 0, 4, false /* pathFound */, 0.0, {});
}

UNIT_TEST(IndexGraph_OnlyTopology_2)
{
  uint32_t const numVertices = 1;
  TestIndexGraphTopology graph(numVertices);
  graph.AddDirectedEdge(0, 0, 100.0);

  double const expectedWeight = 0.0;
  vector<TestEdge> const expectedEdges = {};

  TestTopologyGraph(graph, 0, 0, true /* pathFound */, expectedWeight, expectedEdges);
}

UNIT_TEST(IndexGraph_OnlyTopology_3)
{
  uint32_t const numVertices = 2;
  TestIndexGraphTopology graph(numVertices);

  graph.AddDirectedEdge(0, 1, 1.0);
  graph.AddDirectedEdge(1, 0, 1.0);
  double const expectedWeight = 1.0;
  vector<TestEdge> const expectedEdges = {{0, 1}};

  TestTopologyGraph(graph, 0, 1, true /* pathFound */, expectedWeight, expectedEdges);
}

//      ^ edge1 (-0.002, 0) - (-0.002, 0.002)
//      |
//      |       ^ direction vector (0, 0.001)
//      |       |
//      *-------*------->  edge2 (-0.002, 0) - (0.002, 0)
//          point (0, 0)
//
// Test that a codirectional edge is always better than others.
UNIT_TEST(BestEdgeComparator_OneCodirectionalEdge)
{
  Edge const edge1 = Edge::MakeFake(geometry::MakePointWithAltitudeForTesting({-0.002, 0.0}),
                                    geometry::MakePointWithAltitudeForTesting({-0.002, 0.002}));
  Edge const edge2 = Edge::MakeFake(geometry::MakePointWithAltitudeForTesting({-0.002, 0.0}),
                                    geometry::MakePointWithAltitudeForTesting({0.002, 0.0}));
  IndexRouter::BestEdgeComparator bestEdgeComparator(m2::PointD(0.0, 0.0), m2::PointD(0.0, 0.001));

  TEST_EQUAL(bestEdgeComparator.Compare(edge1, edge2), -1, ());
}

//
//      ^ edge1 (-0.002, 0) - (-0.002, 0.004)
//      |
//      |
//      |
//      ^       ^ edge2 (0, 0) - (0, 0.002)
//      |       |
//      |       ^ direction vector (0, 0.001)
//      |       |
//      *-------*
//          point (0, 0)
//
// Test that if there are two codirectional edges the closest one to |point| is better.
UNIT_TEST(BestEdgeComparator_TwoCodirectionalEdges)
{
  Edge const edge1 = Edge::MakeFake(geometry::MakePointWithAltitudeForTesting({-0.002, 0.0}),
                                    geometry::MakePointWithAltitudeForTesting({-0.002, 0.004}));
  Edge const edge2 = Edge::MakeFake(geometry::MakePointWithAltitudeForTesting({0.0, 0.0}),
                                    geometry::MakePointWithAltitudeForTesting({0.0, 0.002}));
  IndexRouter::BestEdgeComparator bestEdgeComparator(m2::PointD(0.0, 0.0), m2::PointD(0.0, 0.001));

  TEST_EQUAL(bestEdgeComparator.Compare(edge1, edge2), 1, ());
}

//      *--------------->  edge1 (-0.002, 0.002) - (0.002, 0.002)
//
//              ^ direction vector (0, 0.001)
//              |
//      *-------*------->  edge2 (-0.002, 0) - (0.002, 0)
//          point (0, 0)
//
// Test that if two edges are not codirectional the closet one to |point| is better.
UNIT_TEST(BestEdgeComparator_TwoNotCodirectionalEdges)
{
  Edge const edge1 = Edge::MakeFake(geometry::MakePointWithAltitudeForTesting({-0.002, 0.002}),
                                    geometry::MakePointWithAltitudeForTesting({0.002, 0.002}));
  Edge const edge2 = Edge::MakeFake(geometry::MakePointWithAltitudeForTesting({-0.002, 0.0}),
                                    geometry::MakePointWithAltitudeForTesting({0.002, 0.0}));
  IndexRouter::BestEdgeComparator bestEdgeComparator(m2::PointD(0.0, 0.0), m2::PointD(0.0, 0.001));

  TEST_EQUAL(bestEdgeComparator.Compare(edge1, edge2), 1, ());
}

//                                     point(0, 0.001)
//                                         *-->  direction vector (0.001, 0)
//
//  edge1 (-0.002, 0) - (0.002, 0)  <------------->  edge2 (0.002, 0) - (-0.002, 0)
//                                       (0, 0)
//
// Test that if two edges are made by one bidirectional feature the one which have almost the same direction is better.
UNIT_TEST(BestEdgeComparator_TwoEdgesOfOneFeature)
{
  // Please see a note in class Edge definition about start and end point of Edge.
  Edge const edge1 = Edge::MakeFake(geometry::MakePointWithAltitudeForTesting({-0.002, 0.0}),
                                    geometry::MakePointWithAltitudeForTesting({0.002, 0.0}));
  Edge const edge2 = edge1.GetReverseEdge();

  IndexRouter::BestEdgeComparator bestEdgeComparator(m2::PointD(0.0, 0.001), m2::PointD(0.001, 0.0));

  TEST_EQUAL(bestEdgeComparator.Compare(edge1, edge2), -1, ());
  TEST_EQUAL(bestEdgeComparator.Compare(edge2, edge1), 1, ());
}

// Roads
//
//                 R0 *  - - - - -** R1
//              ^                       ^
//           start                    finish
//
//    x:  0     1     2     3     4     5
//
UNIT_TEST(FinishNearZeroEdge)
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();

  loader->AddRoad(0 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {4.0, 0.0}}));
  loader->AddRoad(1 /* featureId */, false, 1.0 /* speed */,
                  RoadGeometry::Points({{4.0, 0.0}, {4.0, 0.0}}));

  vector<Joint> joints;
  joints.emplace_back(MakeJoint({{0 /* featureId */, 1 /* pointId */}, {1, 0}}));

  traffic::TrafficCache const trafficCache;
  shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);
  unique_ptr<WorldGraph> worldGraph = BuildWorldGraph(std::move(loader), estimator, joints);
  auto const start = MakeFakeEnding({Segment(kTestNumMwmId, 0, 0, true /* forward */)},
                                    m2::PointD(1.0, 0.0), *worldGraph);
  auto const finish = MakeFakeEnding({Segment(kTestNumMwmId, 1, 0, false /* forward */)},
                                     m2::PointD(5.0, 0.0), *worldGraph);
  auto starter = MakeStarter(start, finish, *worldGraph);

  vector<m2::PointD> const expectedGeom = {
      {1.0 /* x */, 0.0 /* y */}, {2.0, 0.0}, {4.0, 0.0}, {5.0, 0.0}};
  TestRouteGeometry(*starter, AlgorithmForIndexGraphStarter::Result::OK, expectedGeom);
}
}  // namespace index_graph_test
