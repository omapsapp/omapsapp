#include "testing/testing.hpp"

#include "routing/route.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/turns.hpp"

#include "routing/base/followed_polyline.hpp"

#include "routing/routing_tests/tools.hpp"

#include "platform/location.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include <set>
#include <string>
#include <vector>

namespace route_tests
{
using namespace routing;
using namespace routing::turns;
using namespace std;

// For all test geometry: geometry[0] == geometry[1], since info about 1st point will be lost.
static vector<m2::PointD> const kTestGeometry =
    {{0,0}, {0,0}, {1,1}, {1,2}, {1,3}, {1,4}};
static vector<Segment> const kTestSegments =
    {{0, 0, 0, true},
     {0, 0, 1, true},
     {0, 0, 2, true},
     {0, 0, 3, true},
     {0, 0, 4, true}};
static vector<turns::TurnItem> const kTestTurns(
    {turns::TurnItem(1, turns::CarDirection::None),
     turns::TurnItem(2, turns::CarDirection::TurnLeft),
     turns::TurnItem(3, turns::CarDirection::TurnRight),
     turns::TurnItem(4, turns::CarDirection::None),
     turns::TurnItem(5, turns::CarDirection::ReachedYourDestination)});
static vector<double> const kTestTimes =
    {0.0, 7.0, 10.0, 19.0, 20.0};
static vector<RouteSegment::RoadNameInfo> const kTestNames =
    {{"Street0", "", "", "", "", false},
     {"Street1", "", "", "", "", false},
     {"Street2", "", "", "", "", false},
     {"", "", "", "", "", false},
     {"Street3", "", "", "", "", false}};

void GetTestRouteSegments(vector<m2::PointD> const & routePoints, vector<turns::TurnItem> const & turns,
                          vector<RouteSegment::RoadNameInfo> const & streets, vector<double> const &,
                          vector<RouteSegment> & routeSegments)
{
  RouteSegmentsFrom({}, routePoints, turns, streets, routeSegments);
  FillSegmentInfo(kTestTimes, routeSegments);
}

location::GpsInfo GetGps(double x, double y)
{
  location::GpsInfo info;
  info.m_latitude = mercator::YToLat(y);
  info.m_longitude = mercator::XToLon(x);
  info.m_horizontalAccuracy = 2;
  return info;
}

vector<vector<Segment>> const GetSegments()
{
  auto const segmentsAllReal = kTestSegments;
  vector<Segment> const segmentsAllFake =
      {{kFakeNumMwmId, 0, 0, true},
       {kFakeNumMwmId, 0, 1, true},
       {kFakeNumMwmId, 0, 2, true},
       {kFakeNumMwmId, 0, 3, true},
       {kFakeNumMwmId, 0, 4, true}};
  vector<Segment> const segmentsFakeHeadAndTail =
      {{kFakeNumMwmId, 0, 0, true},
       {0, 0, 1, true},
       {0, 0, 2, true},
       {0, 0, 3, true},
       {kFakeNumMwmId, 0, 4, true}};
  return {segmentsAllReal, segmentsFakeHeadAndTail, segmentsAllFake};
}

UNIT_TEST(AddAbsentCountryToRouteTest)
{
  Route route("TestRouter", 0 /* route id */);
  route.AddAbsentCountry("A");
  route.AddAbsentCountry("A");
  route.AddAbsentCountry("B");
  route.AddAbsentCountry("C");
  route.AddAbsentCountry("B");
  set<string> const & absent = route.GetAbsentCountries();
  TEST(absent.find("A") != absent.end(), ());
  TEST(absent.find("B") != absent.end(), ());
  TEST(absent.find("C") != absent.end(), ());
}

UNIT_TEST(FinshRouteOnSomeDistanceToTheFinishPointTest)
{
  for (auto const vehicleType :
       {VehicleType::Car, VehicleType::Bicycle, VehicleType::Pedestrian, VehicleType::Transit})
  {
    auto const settings = GetRoutingSettings(vehicleType);
    for (auto const & segments : GetSegments())
    {
      Route route("TestRouter", 0 /* route id */);
      route.SetRoutingSettings(settings);

      vector<RouteSegment> routeSegments;
      RouteSegmentsFrom(segments, kTestGeometry, kTestTurns, {}, routeSegments);
      FillSegmentInfo(kTestTimes, routeSegments);
      route.SetRouteSegments(std::move(routeSegments));

      route.SetGeometry(kTestGeometry.begin(), kTestGeometry.end());
      route.SetSubroteAttrs(vector<Route::SubrouteAttrs>(
          {Route::SubrouteAttrs(geometry::PointWithAltitude(kTestGeometry.front(), geometry::kDefaultAltitudeMeters),
                                geometry::PointWithAltitude(kTestGeometry.back(), geometry::kDefaultAltitudeMeters),
                                0, kTestSegments.size())}));

      // The route should be finished at some distance to the finish point.
      double const distToFinish = settings.m_finishToleranceM;

      route.MoveIterator(GetGps(kTestGeometry.back().x, kTestGeometry.back().y - 0.1));
      TEST(!route.IsSubroutePassed(0), ());
      TEST_GREATER(route.GetCurrentDistanceToEndMeters(), distToFinish, ());

      route.MoveIterator(GetGps(kTestGeometry.back().x, kTestGeometry.back().y - 0.02));
      TEST(!route.IsSubroutePassed(0), ());
      TEST_GREATER(route.GetCurrentDistanceToEndMeters(), distToFinish, ());

      // Finish tolerance value for cars is greater then for other vehicle types.
      // The iterator for other types should be moved closer to the finish point.
      if (vehicleType == VehicleType::Car)
        route.MoveIterator(GetGps(kTestGeometry.back().x, kTestGeometry.back().y - 0.00014));
      else
        route.MoveIterator(GetGps(kTestGeometry.back().x, kTestGeometry.back().y - 0.00011));

      TEST(route.IsSubroutePassed(0), ());
      TEST_LESS(route.GetCurrentDistanceToEndMeters(), distToFinish, ());
    }
  }
}

UNIT_TEST(DistanceAndTimeToCurrentTurnTest)
{
  // |curTurn.m_index| is an index of the point of |curTurn| at polyline |route.m_poly|.

  Route route("TestRouter", 0 /* route id */);
  vector<RouteSegment> routeSegments;
  GetTestRouteSegments(kTestGeometry, kTestTurns, {}, kTestTimes, routeSegments);
  route.SetGeometry(kTestGeometry.begin(), kTestGeometry.end());
  vector<turns::TurnItem> turns(kTestTurns);

  route.SetRouteSegments(std::move(routeSegments));

  double distance;
  turns::TurnItem turn;

  {
    // Initial point.
    auto pos = kTestGeometry[0];

    route.GetNearestTurn(distance, turn);
    size_t currentTurnIndex = 2; // Turn with m_index == 1 is None.
    TEST_ALMOST_EQUAL_ABS(distance, mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]), 0.1, ());
    TEST_EQUAL(turn, kTestTurns[currentTurnIndex - 1], ());

    double timePassed = 0;

    double time = route.GetCurrentTimeToEndSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[4] - timePassed, 0.1, ());

    time = route.GetCurrentTimeToNearestTurnSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[currentTurnIndex - 1] - timePassed, 0.1, ());
  }
  {
    // Move between points 1 and 2.
    auto const pos = (kTestGeometry[1] + kTestGeometry[2]) / 2;
    route.MoveIterator(GetGps(pos.x, pos.y));

    route.GetNearestTurn(distance, turn);
    size_t const currentTurnIndex = 2;
    TEST_ALMOST_EQUAL_ABS(distance, mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]), 0.1, ());
    TEST_EQUAL(turn, kTestTurns[currentTurnIndex - 1], ());

    double const timePassed = (kTestTimes[1 - 1] + kTestTimes[2 - 1]) / 2;

    double time = route.GetCurrentTimeToEndSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[4] - timePassed, 0.1, ());

    time = route.GetCurrentTimeToNearestTurnSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[currentTurnIndex - 1] - timePassed, 0.1, ());
  }
  {
    // Move between points 2 and 3.
    auto const pos = kTestGeometry[2] * 0.8 + kTestGeometry[3] * 0.2;
    route.MoveIterator(GetGps(pos.x, pos.y));

    route.GetNearestTurn(distance, turn);
    size_t const currentTurnIndex = 3;
    TEST_ALMOST_EQUAL_ABS(distance, mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]), 0.1, ());
    TEST_EQUAL(turn, kTestTurns[currentTurnIndex - 1], ());

    double const timePassed = 0.8 * kTestTimes[2 - 1] + 0.2 * kTestTimes[3 - 1];

    double time = route.GetCurrentTimeToEndSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[4] - timePassed, 0.1, ());

    time = route.GetCurrentTimeToNearestTurnSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[currentTurnIndex - 1] - timePassed, 0.1, ());
  }
  {
    // Move between points 3 and 4.
    auto const pos = kTestGeometry[3] * 0.3 + kTestGeometry[4] * 0.7;
    route.MoveIterator(GetGps(pos.x, pos.y));

    route.GetNearestTurn(distance, turn);
    size_t const currentTurnIndex = 5; // Turn with m_index == 4 is None.
    TEST_ALMOST_EQUAL_ABS(distance, mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]), 0.1, ());
    TEST_EQUAL(turn, kTestTurns[currentTurnIndex - 1], ());

    double const timePassed = 0.3 * kTestTimes[3 - 1] + 0.7 * kTestTimes[4 - 1];

    double time = route.GetCurrentTimeToEndSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[4] - timePassed, 0.1, ());

    time = route.GetCurrentTimeToNearestTurnSec();
    TEST_ALMOST_EQUAL_ABS(time, kTestTimes[currentTurnIndex - 1] - timePassed, 0.1, ());
  }
}

UNIT_TEST(NextTurnTest)
{
  Route route("TestRouter", 0 /* route id */);
  vector<RouteSegment> routeSegments;
  GetTestRouteSegments(kTestGeometry, kTestTurns, {}, {}, routeSegments);
  route.SetRouteSegments(std::move(routeSegments));
  route.SetGeometry(kTestGeometry.begin(), kTestGeometry.end());

  double distance, nextDistance;
  turns::TurnItem turn, nextTurn;

  {
    // Initial point.
    size_t const currentTurnIndex = 2; // Turn with m_index == 1 is None.
    route.GetNearestTurn(distance, turn);
    TEST_EQUAL(turn, kTestTurns[currentTurnIndex - 1], ());

    size_t const nextTurnIndex = 3;
    route.GetNextTurn(nextDistance, nextTurn);
    TEST_EQUAL(nextTurn, kTestTurns[nextTurnIndex - 1], ());
  }
  {
    // Move between points 1 and 2.
    auto const pos = (kTestGeometry[1] + kTestGeometry[2]) / 2;
    route.MoveIterator(GetGps(pos.x, pos.y));

    size_t const currentTurnIndex = 2;
    route.GetNearestTurn(distance, turn);
    TEST_EQUAL(turn, kTestTurns[currentTurnIndex - 1], ());

    size_t const nextTurnIndex = 3;
    route.GetNextTurn(nextDistance, nextTurn);
    TEST_EQUAL(nextTurn, kTestTurns[nextTurnIndex - 1], ());
  }
  {
    // Move between points 3 and 4.
    auto const pos = (kTestGeometry[3] + kTestGeometry[4]) / 2;
    route.MoveIterator(GetGps(pos.x, pos.y));

    size_t const currentTurnIndex = 5; // Turn with m_index == 4 is None.
    route.GetNearestTurn(distance, turn);
    TEST_EQUAL(turn, kTestTurns[currentTurnIndex - 1], ());

    // nextTurn is absent.
    route.GetNextTurn(nextDistance, nextTurn);
    TEST_EQUAL(nextTurn, turns::TurnItem(), ());
  }
}

UNIT_TEST(NextTurnsTest)
{
  Route route("TestRouter", 0 /* route id */);
  route.SetGeometry(kTestGeometry.begin(), kTestGeometry.end());
  vector<RouteSegment> routeSegments;
  GetTestRouteSegments(kTestGeometry, kTestTurns, {}, {}, routeSegments);
  route.SetRouteSegments(std::move(routeSegments));

  vector<turns::TurnItem> turns(kTestTurns);
  vector<turns::TurnItemDist> turnsDist;

  {
    // Initial point.
    auto const pos = kTestGeometry[0];

    size_t const currentTurnIndex = 2; // Turn with m_index == 1 is None.
    size_t const nextTurnIndex = 3;
    TEST(route.GetNextTurns(turnsDist), ());
    TEST_EQUAL(turnsDist.size(), 2, ());
    double const firstSegLenM = mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]);
    double const secondSegLenM = mercator::DistanceOnEarth(kTestGeometry[currentTurnIndex], kTestGeometry[nextTurnIndex]);
    TEST_EQUAL(turnsDist[0].m_turnItem, kTestTurns[currentTurnIndex - 1], ());
    TEST_EQUAL(turnsDist[1].m_turnItem, kTestTurns[nextTurnIndex - 1], ());
    TEST_ALMOST_EQUAL_ABS(turnsDist[0].m_distMeters, firstSegLenM, 0.1, ());
    TEST_ALMOST_EQUAL_ABS(turnsDist[1].m_distMeters, firstSegLenM + secondSegLenM, 0.1, ());

  }
  {
    // Move between points 1 and 2.
    auto const pos = (kTestGeometry[1] + kTestGeometry[2]) / 2;
    route.MoveIterator(GetGps(pos.x, pos.y));

    size_t const currentTurnIndex = 2;
    size_t const nextTurnIndex = 3;
    TEST(route.GetNextTurns(turnsDist), ());
    TEST_EQUAL(turnsDist.size(), 2, ());
    double const firstSegLenM = mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]);
    double const secondSegLenM = mercator::DistanceOnEarth(kTestGeometry[currentTurnIndex], kTestGeometry[nextTurnIndex]);
    TEST_EQUAL(turnsDist[0].m_turnItem, kTestTurns[currentTurnIndex - 1], ());
    TEST_EQUAL(turnsDist[1].m_turnItem, kTestTurns[nextTurnIndex - 1], ());
    TEST_ALMOST_EQUAL_ABS(turnsDist[0].m_distMeters, firstSegLenM, 0.1, ());
    TEST_ALMOST_EQUAL_ABS(turnsDist[1].m_distMeters, firstSegLenM + secondSegLenM, 0.1, ());
  }
  {
    // Move between points 2 and 3.
    auto const pos = (kTestGeometry[2] + kTestGeometry[3]) / 2;
    route.MoveIterator(GetGps(pos.x, pos.y));

    size_t const currentTurnIndex = 3;
    size_t const nextTurnIndex = 5; // Turn with m_index == 4 is None.
    TEST(route.GetNextTurns(turnsDist), ());
    TEST_EQUAL(turnsDist.size(), 2, ());
    double const firstSegLenM = mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]);
    double const secondSegLenM = mercator::DistanceOnEarth(kTestGeometry[currentTurnIndex], kTestGeometry[nextTurnIndex]);
    TEST_EQUAL(turnsDist[0].m_turnItem, kTestTurns[currentTurnIndex - 1], ());
    TEST_EQUAL(turnsDist[1].m_turnItem, kTestTurns[nextTurnIndex - 1], ());
    TEST_ALMOST_EQUAL_ABS(turnsDist[0].m_distMeters, firstSegLenM, 0.1, ());
    TEST_ALMOST_EQUAL_ABS(turnsDist[1].m_distMeters, firstSegLenM + secondSegLenM, 0.1, ());
  }
  {
    // Move between points 3 and 4.
    auto const pos = (kTestGeometry[3] + kTestGeometry[4]) / 2;
    route.MoveIterator(GetGps(pos.x, pos.y));

    size_t const currentTurnIndex = 5; // Turn with m_index == 4 is None.
    // nextTurn is absent.
    TEST(route.GetNextTurns(turnsDist), ());
    double const firstSegLenM = mercator::DistanceOnEarth(pos, kTestGeometry[currentTurnIndex]);
    TEST_EQUAL(turnsDist[0].m_turnItem, kTestTurns[currentTurnIndex - 1], ());
    TEST_ALMOST_EQUAL_ABS(turnsDist[0].m_distMeters, firstSegLenM, 0.1, ());
  }
}

//   0.0002          *--------*
//                   |        |
//                   |        |
//        Finish     |        |
//   0.0001  *----------------*
//                   |
//                   |
//                   |
//        0          * Start
//           0  0.0001    0.0002
//
UNIT_TEST(SelfIntersectedRouteMatchingTest)
{
  vector<m2::PointD> const kRouteGeometry =
      {{0.0001, 0.0},
       {0.0001, 0.0},
       {0.0001, 0.0002},
       {0.0002, 0.0002},
       {0.0002, 0.0001},
       {0.0, 0.0001}};
  double constexpr kRoundingErrorMeters = 0.001;

  Route route("TestRouter", 0 /* route id */);
  route.SetGeometry(kRouteGeometry.begin(), kRouteGeometry.end());

  vector<RouteSegment> routeSegments;
  GetTestRouteSegments(kRouteGeometry, {}, {}, {}, routeSegments);
  route.SetRouteSegments(std::move(routeSegments));

  auto const testMachedPos = [&](location::GpsInfo const & pos,
                                 location::GpsInfo const & expectedMatchingPos,
                                 size_t expectedIndexInRoute) {
    location::RouteMatchingInfo routeMatchingInfo;
    route.MoveIterator(pos);
    location::GpsInfo matchedPos = pos;
    route.MatchLocationToRoute(matchedPos, routeMatchingInfo);
    TEST_LESS(mercator::DistanceOnEarth(
                  m2::PointD(matchedPos.m_latitude, matchedPos.m_longitude),
                  m2::PointD(expectedMatchingPos.m_latitude, expectedMatchingPos.m_longitude)),
              kRoundingErrorMeters, ());
    TEST_EQUAL(max(size_t(1), routeMatchingInfo.GetIndexInRoute()), expectedIndexInRoute, ());
  };

  // Moving along the route from start point.
  location::GpsInfo const pos1(GetGps(0.0001, 0.0));
  testMachedPos(pos1, pos1, 1 /* expectedIndexInRoute */);
  location::GpsInfo const pos2(GetGps(0.0001, 0.00005));
  testMachedPos(pos2, pos2, 1 /* expectedIndexInRoute */);

  // Moving around the self intersection and checking that position is matched to the first segment of the route.
  location::GpsInfo const selfIntersectionPos(GetGps(0.0001, 0.0001));
  location::GpsInfo const pos3(GetGps(0.00005, 0.0001));
  testMachedPos(pos3, selfIntersectionPos, 1 /* expectedIndexInRoute */);
  location::GpsInfo const pos4(GetGps(0.00015, 0.0001));
  testMachedPos(pos4, selfIntersectionPos, 1 /* expectedIndexInRoute */);

  // Continue moving along the route.
  location::GpsInfo const pos5(GetGps(0.00011, 0.0002));
  testMachedPos(pos5, pos5, 2 /* expectedIndexInRoute */);
  location::GpsInfo const pos6(GetGps(0.0002, 0.00019));
  testMachedPos(pos6, pos6, 3 /* expectedIndexInRoute */);
  location::GpsInfo const pos7(GetGps(0.00019, 0.0001));
  testMachedPos(pos7, pos7, 4 /* expectedIndexInRoute */);

  // Moving around the self intersection and checking that position is matched to the last segment of the route.
  location::GpsInfo const pos8(GetGps(0.0001, 0.00005));
  testMachedPos(pos8, selfIntersectionPos, 4 /* expectedIndexInRoute */);
  location::GpsInfo const pos9(GetGps(0.0001, 0.00015));
  testMachedPos(pos9, selfIntersectionPos, 4 /* expectedIndexInRoute */);
}

UNIT_TEST(RouteNameTest)
{
  Route route("TestRouter", 0 /* route id */);

  route.SetGeometry(kTestGeometry.begin(), kTestGeometry.end());
  vector<RouteSegment> routeSegments;
  GetTestRouteSegments(kTestGeometry, kTestTurns, kTestNames, {}, routeSegments);
  route.SetRouteSegments(std::move(routeSegments));

  RouteSegment::RoadNameInfo roadNameInfo;
  route.GetCurrentStreetName(roadNameInfo);
  TEST_EQUAL(roadNameInfo.m_name, "Street0", (roadNameInfo.m_name));

  route.GetClosestStreetNameAfterIdx(1, roadNameInfo);
  TEST_EQUAL(roadNameInfo.m_name, "Street1", (roadNameInfo.m_name));

  route.GetClosestStreetNameAfterIdx(2, roadNameInfo);
  TEST_EQUAL(roadNameInfo.m_name, "Street2", (roadNameInfo.m_name));

  route.GetClosestStreetNameAfterIdx(3, roadNameInfo);
  TEST_EQUAL(roadNameInfo.m_name, "Street3", (roadNameInfo.m_name));

  route.GetClosestStreetNameAfterIdx(4, roadNameInfo);
  TEST_EQUAL(roadNameInfo.m_name, "Street3", (roadNameInfo.m_name));

  location::GpsInfo const pos(GetGps(1.0, 2.0));
  route.MoveIterator(pos);
  route.GetCurrentStreetName(roadNameInfo);
  TEST_EQUAL(roadNameInfo.m_name, "Street2", (roadNameInfo.m_name));
}
}  // namespace route_tests
