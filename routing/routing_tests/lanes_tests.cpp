#include "testing/testing.hpp"

#include "routing/lanes.hpp"
#include "routing/car_directions.hpp"
/*
namespace routing::turns::lanes::test
{
UNIT_TEST(TestSplitLanes)
{
  vector<string> result;
  SplitLanes("through|through|through|through;right", '|', result);
  vector<string> const expected1 = {"through", "through", "through", "through;right"};
  TEST_EQUAL(result, expected1, ());

  SplitLanes("adsjkddfasui8747&sxdsdlad8\"\'", '|', result);
  TEST_EQUAL(result, vector<string>({"adsjkddfasui8747&sxdsdlad8\"\'"}), ());

  SplitLanes("|||||||", '|', result);
  vector<string> expected2 = {"", "", "", "", "", "", ""};
  TEST_EQUAL(result, expected2, ());
}

UNIT_TEST(TestParseSingleLane)
{
  LaneWays result;
  TEST(ParseSingleLane("through;right", ';', result), ());
  LaneWays const expected1 = {LaneWay::Through, LaneWay::Right};
  TEST_EQUAL(result, expected1, ());

  TEST(!ParseSingleLane("through;Right", ';', result), ());

  TEST(!ParseSingleLane("through ;right", ';', result), ());
  TEST_EQUAL(result.size(), 0, ());

  TEST(!ParseSingleLane("SD32kk*887;;", ';', result), ());
  TEST_EQUAL(result.size(), 0, ());

  TEST(!ParseSingleLane("Что-то на кириллице", ';', result), ());
  TEST_EQUAL(result.size(), 0, ());

  TEST(!ParseSingleLane("משהו בעברית", ';', result), ());
  TEST_EQUAL(result.size(), 0, ());

  TEST(ParseSingleLane("left;through", ';', result), ());
  LaneWays const expected2 = {LaneWay::Left, LaneWay::Through};
  TEST_EQUAL(result, expected2, ());

  TEST(ParseSingleLane("left", ';', result), ());
  TEST_EQUAL(result.size(), 1, ());
  TEST_EQUAL(result[0], LaneWay::Left, ());

  TEST(ParseSingleLane("left;", ';', result), ());
  LaneWays const expected3 = {LaneWay::Left, LaneWay::None};
  TEST_EQUAL(result, expected3, ());

  TEST(ParseSingleLane(";", ';', result), ());
  LaneWays const expected4 = {LaneWay::None, LaneWay::None};
  TEST_EQUAL(result, expected4, ());

  TEST(ParseSingleLane("", ';', result), ());
  LaneWays const expected5 = {LaneWay::None};
  TEST_EQUAL(result, expected5, ());
}

UNIT_TEST(TestParseLanes)
{
  vector<SingleLaneInfo> result;
  TEST(ParseLanes("through|through|through|through;right", result), ());
  vector<SingleLaneInfo> const expected1 = {{LaneWay::Through},
                                            {LaneWay::Through},
                                            {LaneWay::Through},
                                            {LaneWay::Through, LaneWay::Right}};
  TEST_EQUAL(result, expected1, ());

  TEST(ParseLanes("left|left;through|through|through", result), ());
  vector<SingleLaneInfo> const expected2 = {
    {LaneWay::Left}, {LaneWay::Left, LaneWay::Through}, {LaneWay::Through}, {LaneWay::Through}};
  TEST_EQUAL(result, expected2, ());

  TEST(ParseLanes("left|through|through", result), ());
  vector<SingleLaneInfo> const expected3 = {
    {LaneWay::Left}, {LaneWay::Through}, {LaneWay::Through}};
  TEST_EQUAL(result, expected3, ());

  TEST(ParseLanes("left|le  ft|   through|through   |  right", result), ());
  vector<SingleLaneInfo> const expected4 = {
    {LaneWay::Left}, {LaneWay::Left}, {LaneWay::Through}, {LaneWay::Through}, {LaneWay::Right}};
  TEST_EQUAL(result, expected4, ());

  TEST(ParseLanes("left|Left|through|througH|right", result), ());
  vector<SingleLaneInfo> const expected5 = {
    {LaneWay::Left}, {LaneWay::Left}, {LaneWay::Through}, {LaneWay::Through}, {LaneWay::Right}};
  TEST_EQUAL(result, expected5, ());

  TEST(ParseLanes("left|Left|through|througH|through;right;sharp_rIght", result), ());
  vector<SingleLaneInfo> const expected6 = {
    {LaneWay::Left},
    {LaneWay::Left},
    {LaneWay::Through},
    {LaneWay::Through},
    {LaneWay::Through, LaneWay::Right, LaneWay::SharpRight}};
  TEST_EQUAL(result, expected6, ());

  TEST(!ParseLanes("left|Leftt|through|througH|right", result), ());
  TEST_EQUAL(result.size(), 0, ());

  TEST(!ParseLanes("Что-то на кириллице", result), ());
  TEST_EQUAL(result.size(), 0, ());

  TEST(!ParseLanes("משהו בעברית", result), ());
  TEST_EQUAL(result.size(), 0, ());

  TEST(ParseLanes("left |Left|through|througH|right", result), ());
  vector<SingleLaneInfo> const expected7 = {
    {LaneWay::Left}, {LaneWay::Left}, {LaneWay::Through}, {LaneWay::Through}, {LaneWay::Right}};
  TEST_EQUAL(result, expected7, ());

  TEST(ParseLanes("|||||slight_right", result), ());
  vector<SingleLaneInfo> const expected8 = {
    {LaneWay::None},
    {LaneWay::None},
    {LaneWay::None},
    {LaneWay::None},
    {LaneWay::None},
    {LaneWay::SlightRight}
  };
  TEST_EQUAL(result, expected8, ());
}

UNIT_TEST(TestIsLaneWayConformedTurnDirection)
{
  TEST(IsLaneWayConformedTurnDirection(LaneWay::Left, CarDirection::TurnLeft), ());
  TEST(IsLaneWayConformedTurnDirection(LaneWay::Right, CarDirection::TurnRight), ());
  TEST(IsLaneWayConformedTurnDirection(LaneWay::SlightLeft, CarDirection::TurnSlightLeft), ());
  TEST(IsLaneWayConformedTurnDirection(LaneWay::SharpRight, CarDirection::TurnSharpRight), ());
  TEST(IsLaneWayConformedTurnDirection(LaneWay::Reverse, CarDirection::UTurnLeft), ());
  TEST(IsLaneWayConformedTurnDirection(LaneWay::Reverse, CarDirection::UTurnRight), ());
  TEST(IsLaneWayConformedTurnDirection(LaneWay::Through, CarDirection::GoStraight), ());

  TEST(!IsLaneWayConformedTurnDirection(LaneWay::Left, CarDirection::TurnSlightLeft), ());
  TEST(!IsLaneWayConformedTurnDirection(LaneWay::Right, CarDirection::TurnSharpRight), ());
  TEST(!IsLaneWayConformedTurnDirection(LaneWay::SlightLeft, CarDirection::GoStraight), ());
  TEST(!IsLaneWayConformedTurnDirection(LaneWay::SharpRight, CarDirection::None), ());
  TEST(!IsLaneWayConformedTurnDirection(LaneWay::Reverse, CarDirection::TurnLeft), ());
  TEST(!IsLaneWayConformedTurnDirection(LaneWay::None, CarDirection::ReachedYourDestination), ());
}

UNIT_TEST(TestIsLaneWayConformedTurnDirectionApproximately)
{
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::Left, CarDirection::TurnSharpLeft), ());
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::Left, CarDirection::TurnSlightLeft), ());
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::Right, CarDirection::TurnSharpRight), ());
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::Right, CarDirection::TurnRight), ());
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::Reverse, CarDirection::UTurnLeft), ());
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::Reverse, CarDirection::UTurnRight), ());
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::SlightLeft, CarDirection::GoStraight), ());
  TEST(IsLaneWayConformedTurnDirectionApproximately(LaneWay::SlightRight, CarDirection::GoStraight), ());

  TEST(!IsLaneWayConformedTurnDirectionApproximately(LaneWay::SharpLeft, CarDirection::UTurnLeft), ());
  TEST(!IsLaneWayConformedTurnDirectionApproximately(LaneWay::SharpLeft, CarDirection::UTurnRight), ());
  TEST(!IsLaneWayConformedTurnDirectionApproximately(LaneWay::SharpRight, CarDirection::UTurnLeft), ());
  TEST(!IsLaneWayConformedTurnDirectionApproximately(LaneWay::SharpRight, CarDirection::UTurnRight), ());
  TEST(!IsLaneWayConformedTurnDirection(LaneWay::Through, CarDirection::ReachedYourDestination), ());
  TEST(!IsLaneWayConformedTurnDirectionApproximately(LaneWay::Through, CarDirection::TurnRight), ());
  TEST(!IsLaneWayConformedTurnDirectionApproximately(LaneWay::SlightRight,
                                                     CarDirection::TurnSharpLeft), ());
}

UNIT_TEST(TestAddingActiveLaneInformation)
{
  vector<TurnItem> turns =
      {{1, CarDirection::GoStraight},
       {2, CarDirection::TurnLeft},
       {3, CarDirection::TurnRight},
       {4, CarDirection::ReachedYourDestination}};

  turns[0].m_lanes.push_back({LaneWay::Left, LaneWay::Through});
  turns[0].m_lanes.push_back({LaneWay::Right});

  turns[1].m_lanes.push_back({LaneWay::SlightLeft});
  turns[1].m_lanes.push_back({LaneWay::Through});
  turns[1].m_lanes.push_back({LaneWay::None});

  turns[2].m_lanes.push_back({LaneWay::Left, LaneWay::SharpLeft});
  turns[2].m_lanes.push_back({LaneWay::None});

  vector<RouteSegment> routeSegments;
  RouteSegmentsFrom({}, {}, turns, {}, routeSegments);
  SelectRecommendedLanes(routeSegments);

  TEST(routeSegments[0].GetTurn().m_lanes[0].m_isRecommended, ());
  TEST(!routeSegments[0].GetTurn().m_lanes[1].m_isRecommended, ());

  TEST(routeSegments[1].GetTurn().m_lanes[0].m_isRecommended, ());
  TEST(!routeSegments[1].GetTurn().m_lanes[1].m_isRecommended, ());
  TEST(!routeSegments[1].GetTurn().m_lanes[2].m_isRecommended, ());

  TEST(!routeSegments[2].GetTurn().m_lanes[0].m_isRecommended, ());
  TEST(routeSegments[2].GetTurn().m_lanes[1].m_isRecommended, ());
}
}
*/