#include "routing/car_directions.hpp"

#include "routing/turns.hpp"
#include "routing/turns_generator.hpp"
#include "routing/turns_generator_utils.hpp"

#include "geometry/angles.hpp"

namespace routing
{
using namespace std;
using namespace turns;
using namespace ftypes;

CarDirectionsEngine::CarDirectionsEngine(MwmDataSource & dataSource, shared_ptr<NumMwmIds> numMwmIds)
  : DirectionsEngine(dataSource, std::move(numMwmIds))
{
}

void CarDirectionsEngine::FixupTurns(vector<RouteSegment> & routeSegments)
{
  FixupCarTurns(routeSegments);
}

void FixupCarTurns(vector<RouteSegment> & routeSegments)
{
  double constexpr kMergeDistMeters = 15.0;
  // For turns that are not EnterRoundAbout/ExitRoundAbout exitNum is always equal to zero.
  // If a turn is EnterRoundAbout exitNum is a number of turns between two junctions:
  // (1) the route enters to the roundabout;
  // (2) the route leaves the roundabout;
  uint32_t exitNum = 0;
  size_t const kInvalidEnter = numeric_limits<size_t>::max();
  size_t enterRoundAbout = kInvalidEnter;

  for (size_t idx = 0; idx < routeSegments.size(); ++idx)
  {
    auto & t = routeSegments[idx].GetTurn();
    if (t.IsTurnNone())
      continue;

    if (enterRoundAbout != kInvalidEnter && t.m_turn != CarDirection::StayOnRoundAbout
        && t.m_turn != CarDirection::LeaveRoundAbout && t.m_turn != CarDirection::ReachedYourDestination)
    {
      ASSERT(false, ("Only StayOnRoundAbout, LeaveRoundAbout or ReachedYourDestination are expected after EnterRoundAbout."));
      exitNum = 0;
      enterRoundAbout = kInvalidEnter;
    }
    else if (t.m_turn == CarDirection::EnterRoundAbout)
    {
      ASSERT(enterRoundAbout == kInvalidEnter, ("It's not expected to find new EnterRoundAbout until previous EnterRoundAbout was leaved."));
      enterRoundAbout = idx;
      ASSERT(exitNum == 0, ("exitNum is reset at start and after LeaveRoundAbout."));
      exitNum = t.m_exitNum; // Normally it is 0, but sometimes it can be 1.
    }
    else if (t.m_turn == CarDirection::StayOnRoundAbout)
    {
      ++exitNum;
      routeSegments[idx].ClearTurn();
      continue;
    }
    else if (t.m_turn == CarDirection::LeaveRoundAbout)
    {
      // It's possible for car to be on roundabout without entering it
      // if route calculation started at roundabout (e.g. if user made full turn on roundabout).
      if (enterRoundAbout != kInvalidEnter)
        routeSegments[enterRoundAbout].SetTurnExits(exitNum + 1);
      routeSegments[idx].SetTurnExits(exitNum + 1); // For LeaveRoundAbout turn.
      enterRoundAbout = kInvalidEnter;
      exitNum = 0;
    }

    // Merging turns which are closed to each other under some circumstance.
    // distance(turnsDir[idx - 1].m_index, turnsDir[idx].m_index) < kMergeDistMeters
    // means the distance in meters between the former turn (idx - 1)
    // and the current turn (idx).
    if (idx > 0 && IsStayOnRoad(routeSegments[idx - 1].GetTurn().m_turn) &&
        IsLeftOrRightTurn(t.m_turn))
    {
      auto const & junction = routeSegments[idx].GetJunction();
      auto const & prevJunction = routeSegments[idx - 1].GetJunction();
      if (mercator::DistanceOnEarth(junction.GetPoint(), prevJunction.GetPoint()) < kMergeDistMeters)
        routeSegments[idx - 1].ClearTurn();
    }
  }
  lanes::SelectRecommendedLanes(routeSegments);
}

void GetTurnDirectionBasic(IRoutingResult const & result, size_t const outgoingSegmentIndex,
                           NumMwmIds const & numMwmIds, RoutingSettings const & vehicleSettings,
                           TurnItem & turn);

size_t CarDirectionsEngine::GetTurnDirection(IRoutingResult const & result, size_t const outgoingSegmentIndex,
                                             NumMwmIds const & numMwmIds,
                                             RoutingSettings const & vehicleSettings, TurnItem & turnItem)
{
  if (outgoingSegmentIndex == result.GetSegments().size())
  {
    turnItem.m_turn = CarDirection::ReachedYourDestination;
    return 0;
  }

  // This is for jump from initial point to start of the route. No direction is given.
  /// @todo Sometimes results of GetPossibleTurns are empty, sometimes are invalid.
  /// The best will be to fix GetPossibleTurns(). It will allow us to use following approach.
  /// E.g. Google Maps until you reach the destination will guide you to go to the left or to the right of the first road.
  if (outgoingSegmentIndex == 2) // The same as turnItem.m_index == 2.
    return 0;

  size_t skipTurnSegments = CheckUTurnOnRoute(result, outgoingSegmentIndex, numMwmIds, vehicleSettings, turnItem);

  if (turnItem.m_turn == CarDirection::None)
    GetTurnDirectionBasic(result, outgoingSegmentIndex, numMwmIds, vehicleSettings, turnItem);

  // Lane information.
  if (turnItem.m_turn != CarDirection::None)
  {
    auto const & loadedSegments = result.GetSegments();
    auto const & ingoingSegment = loadedSegments[outgoingSegmentIndex - 1];
    turnItem.m_lanes = ingoingSegment.m_lanes;
  }

  return skipTurnSegments;
}

/*!
 * \brief Calculates a turn instruction if the ingoing edge or (and) the outgoing edge belongs to a
 * roundabout.
 * \return Returns one of the following results:
 * - TurnDirection::EnterRoundAbout if the ingoing edge does not belong to a roundabout
 *   and the outgoing edge belongs to a roundabout.
 * - TurnDirection::StayOnRoundAbout if the ingoing edge and the outgoing edge belong to a
 * roundabout
 *   and there is a reasonalbe way to leave the junction besides the outgoing edge.
 *   This function does not return TurnDirection::StayOnRoundAbout for small ways to leave the
 * roundabout.
 * - TurnDirection::NoTurn if the ingoing edge and the outgoing edge belong to a roundabout
 *   (a) and there is a single way (outgoing edge) to leave the junction.
 *   (b) and there is a way(s) besides outgoing edge to leave the junction (the roundabout)
 *       but it is (they are) relevantly small.
 */
CarDirection GetRoundaboutDirectionBasic(bool isIngoingEdgeRoundabout, bool isOutgoingEdgeRoundabout,
                                         bool isMultiTurnJunction, bool keepTurnByHighwayClass)
{
  if (isIngoingEdgeRoundabout && isOutgoingEdgeRoundabout)
  {
    if (isMultiTurnJunction)
      return keepTurnByHighwayClass ? CarDirection::StayOnRoundAbout : CarDirection::None;
    return CarDirection::None;
  }

  if (!isIngoingEdgeRoundabout && isOutgoingEdgeRoundabout)
    return CarDirection::EnterRoundAbout;

  if (isIngoingEdgeRoundabout && !isOutgoingEdgeRoundabout)
    return CarDirection::LeaveRoundAbout;

  ASSERT(false, ());
  return CarDirection::None;
}

/*!
 * \brief Returns false when other possible turns leads to service roads;
 */
bool KeepRoundaboutTurnByHighwayClass(TurnCandidates const & possibleTurns,
                                      TurnInfo const & turnInfo, NumMwmIds const & numMwmIds)
{
  Segment firstOutgoingSegment;
  turnInfo.m_outgoing->m_segmentRange.GetFirstSegment(numMwmIds, firstOutgoingSegment);

  for (auto const & t : possibleTurns.candidates)
  {
    if (t.m_segment == firstOutgoingSegment)
      continue;
    // For roundabouts of Tertiary and less significant class count every road.
    // For more significant roundabouts - ignore service roads.
    if (turnInfo.m_outgoing->m_highwayClass >= HighwayClass::Tertiary || t.m_highwayClass != HighwayClass::Service)
      return true;
  }
  return false;
}

CarDirection GetRoundaboutDirection(TurnInfo const & turnInfo, TurnCandidates const & nodes, NumMwmIds const & numMwmIds)
{
  bool const keepTurnByHighwayClass = KeepRoundaboutTurnByHighwayClass(nodes, turnInfo, numMwmIds);
  return GetRoundaboutDirectionBasic(turnInfo.m_ingoing->m_onRoundabout, turnInfo.m_outgoing->m_onRoundabout,
                                nodes.candidates.size() > 1, keepTurnByHighwayClass);
}

/// \brief Return |CarDirection::ExitHighwayToRight| or |CarDirection::ExitHighwayToLeft|
/// or return |CarDirection::None| if no exit is detected.
/// \note The function makes a decision about turn based on geometry of the route and turn
/// candidates, so it works correctly for both left and right hand traffic.
CarDirection TryToGetExitDirection(TurnCandidates const & possibleTurns, TurnInfo const & turnInfo,
                                   Segment const & firstOutgoingSeg, CarDirection const intermediateDirection)
{
  if (!possibleTurns.isCandidatesAngleValid)
    return CarDirection::None;

  if (!IsHighway(turnInfo.m_ingoing->m_highwayClass, turnInfo.m_ingoing->m_isLink))
    return CarDirection::None;

  if (!turnInfo.m_outgoing->m_isLink && !(IsSmallRoad(turnInfo.m_outgoing->m_highwayClass) && IsGoStraightOrSlightTurn(intermediateDirection)))
    return CarDirection::None;

  // At this point it is known that the route goes form a highway to a link road or to a small road
  // which has a slight angle with the highway.

  // Considering cases when the route goes from a highway to a link or a small road.
  // Checking all turn candidates (sorted by their angles) and looking for the road which is a
  // continuation of the ingoing segment. If the continuation is on the right hand of the route
  // it's an exit to the left. If the continuation is on the left hand of the route
  // it's an exit to the right.
  // Note. The angle which is used for sorting turn candidates in |possibleTurns.candidates|
  // is a counterclockwise angle between the ingoing route edge and corresponding candidate.
  // For left turns the angle is less than zero and for right ones it is more than zero.
  bool isCandidateBeforeOutgoing = true;
  bool isHighwayCandidateBeforeOutgoing = true;
  size_t highwayCandidateNumber = 0;

  for (auto const & c : possibleTurns.candidates)
  {
    if (c.m_segment == firstOutgoingSeg)
    {
      isCandidateBeforeOutgoing = false;
      continue;
    }

    if (IsHighway(c.m_highwayClass, c.m_isLink))
    {
      ++highwayCandidateNumber;
      if (highwayCandidateNumber >= 2)
        return CarDirection::None; // There are two or more highway candidates from the junction.
      isHighwayCandidateBeforeOutgoing = isCandidateBeforeOutgoing;
    }
  }
  if (highwayCandidateNumber == 1)
  {
    return isHighwayCandidateBeforeOutgoing ? CarDirection::ExitHighwayToRight
                                            : CarDirection::ExitHighwayToLeft;
  }
  return CarDirection::None;
}

// Min difference of route and alternative turn abs angles in degrees
// to ignore alternative turn (when route direction is GoStraight).
double constexpr kMinAbsAngleDiffForStraightRoute = 25.0;

// Min difference of route and alternative turn abs angles in degrees
// to ignore alternative turn (when route direction is bigger and GoStraight).
double constexpr kMinAbsAngleDiffForBigStraightRoute = 5;

// Min difference of route and alternative turn abs angles in degrees
// to ignore this alternative candidate (when alternative road is the same or smaller).
double constexpr kMinAbsAngleDiffForSameOrSmallerRoad = 35.0;

/// * \returns true when
/// * - the route leads from one big road to another one;
/// * - and the other possible turns lead to small roads or these turns too sharp.
/// * Returns false otherwise.
/// \param routeDirection is route direction.
/// \param routeAngle is route angle.
/// \param turnCandidates is all possible ways out from a junction except uturn.
/// \param turnInfo is information about ingoing and outgoing segments of the route.
bool CanDiscardTurnByHighwayClassOrAngles(CarDirection const routeDirection,
                                          double const routeAngle,
                                          vector<TurnCandidate> const & turnCandidates,
                                          TurnInfo const & turnInfo,
                                          NumMwmIds const & numMwmIds)
{
  if (!IsGoStraightOrSlightTurn(routeDirection))
    return false;

  if (turnCandidates.size() < 2)
    return true;

  HighwayClass outgoingRouteRoadClass = turnInfo.m_outgoing->m_highwayClass;
  HighwayClass ingoingRouteRoadClass = turnInfo.m_ingoing->m_highwayClass;

  Segment firstOutgoingSegment;
  turnInfo.m_outgoing->m_segmentRange.GetFirstSegment(numMwmIds, firstOutgoingSegment);

  for (auto const & t : turnCandidates)
  {
    // Let's consider all outgoing segments except for route outgoing segment.
    if (t.m_segment == firstOutgoingSegment)
      continue;

    // Min difference of route and alternative turn abs angles in degrees
    // to ignore this alternative candidate (when route goes from non-link to link).
    double constexpr kMinAbsAngleDiffForGoLink = 70.0;

    // If route goes from non-link to link and there is not too sharp candidate then turn can't be discarded.
    if (!turnInfo.m_ingoing->m_isLink && turnInfo.m_outgoing->m_isLink && abs(t.m_angle) < abs(routeAngle) + kMinAbsAngleDiffForGoLink)
      return false;

    HighwayClass candidateRoadClass = t.m_highwayClass;

    // If outgoing route road is significantly larger than candidate, the candidate can be ignored.
    if (CalcDiffRoadClasses(outgoingRouteRoadClass, candidateRoadClass) <= kMinHighwayClassDiff)
      continue;

    // If outgoing route's road is significantly larger than candidate's service road, the candidate can be ignored.
    if (CalcDiffRoadClasses(outgoingRouteRoadClass, candidateRoadClass) <= kMinHighwayClassDiffForService && candidateRoadClass == HighwayClass::Service)
      continue;

    // If igngoing and outgoing edges are not links
    // and outgoing route road is the same or large than ingoing
    // then candidate-link can be ignored.
    if (!turnInfo.m_ingoing->m_isLink && !turnInfo.m_outgoing->m_isLink &&
        CalcDiffRoadClasses(outgoingRouteRoadClass, ingoingRouteRoadClass) <= 0 &&
        t.m_isLink)
      continue;

    // If alternative cadidate's road size is the same or smaller
    // and it's angle is sharp enough compared to the route it can be ignored.
    if (CalcDiffRoadClasses(outgoingRouteRoadClass, candidateRoadClass) <= 0 &&
        abs(t.m_angle) > abs(routeAngle) + kMinAbsAngleDiffForSameOrSmallerRoad)
      continue;

    if (routeDirection == CarDirection::GoStraight)
    {
      // If alternative cadidate's road size is smaller
      // and it's angle is not very close to the route's one - it can be ignored.
      if (CalcDiffRoadClasses(outgoingRouteRoadClass, candidateRoadClass) < 0 &&
          abs(t.m_angle) > abs(routeAngle) + kMinAbsAngleDiffForBigStraightRoute)
        continue;

      // If outgoing route road is the same or large than ingoing
      // and candidate's angle is sharp enough compared to the route it can be ignored.
      if (CalcDiffRoadClasses(outgoingRouteRoadClass, ingoingRouteRoadClass) <= 0 &&
          abs(t.m_angle) > abs(routeAngle) + kMinAbsAngleDiffForStraightRoute)
        continue;
    }

    return false;
  }
  return true;
}

/*!
 * \brief Corrects |turn.m_turn| if |turn.m_turn == CarDirection::GoStraight|.
 * If the other way is not sharp enough, turn.m_turn is set to |turnToSet|.
 */
void CorrectGoStraight(TurnCandidate const & notRouteCandidate, double const routeAngle, CarDirection const & turnToSet,
                       TurnItem & turn)
{
  if (turn.m_turn != CarDirection::GoStraight)
    return;

  // Correct turn if alternative cadidate's angle is not sharp enough compared to the route.
  if (abs(notRouteCandidate.m_angle) < abs(routeAngle) + kMinAbsAngleDiffForStraightRoute)
  {
    LOG(LDEBUG, ("Turn: ", turn.m_index, " GoStraight correction."));
    turn.m_turn = turnToSet;
  }
}

// If the route goes along the rightmost or the leftmost way among available ones:
// 1. RightmostDirection or LeftmostDirection is selected
// 2. If the turn direction is |CarDirection::GoStraight|
// and there's another not sharp enough turn
// GoStraight is corrected to TurnSlightRight/TurnSlightLeft
// to avoid ambiguity for GoStraight direction: 2 or more almost straight turns.
void CorrectRightmostAndLeftmost(vector<TurnCandidate> const & turnCandidates,
                                 Segment const & firstOutgoingSeg, double const turnAngle,
                                 TurnItem & turn)
{
  // turnCandidates are sorted by angle from leftmost to rightmost.
  // Normally no duplicates should be found. But if they are present we can't identify the leftmost/rightmost by order.
  if (adjacent_find(turnCandidates.begin(), turnCandidates.end(), base::EqualsBy(&TurnCandidate::m_angle)) != turnCandidates.end())
  {
    LOG(LWARNING, ("nodes.candidates are not expected to have same m_angle."));
    return;
  }

  double constexpr kMaxAbsAngleConsideredLeftOrRightMost = 90;

  // Go from left to right to findout if the route goes through the leftmost candidate and fixes can be applied.
  // Other candidates which are sharper than kMaxAbsAngleConsideredLeftOrRightMost are ignored.
  for (auto candidate = turnCandidates.begin(); candidate != turnCandidates.end(); ++candidate)
  {
    if (candidate->m_segment == firstOutgoingSeg && candidate + 1 != turnCandidates.end())
    {
      // The route goes along the leftmost candidate.
      turn.m_turn = LeftmostDirection(turnAngle);
      if (IntermediateDirection(turnAngle) != turn.m_turn)
        LOG(LDEBUG, ("Turn: ", turn.m_index, " LeftmostDirection correction."));
      // Compare with the next candidate to the right.
      CorrectGoStraight(*(candidate + 1), candidate->m_angle, CarDirection::TurnSlightLeft, turn);
      break;
    }
    // Check if this candidate is considered as leftmost as not too sharp.
    // If yes - this candidate is leftmost, not route's one.
    if (candidate->m_angle > -kMaxAbsAngleConsideredLeftOrRightMost)
      break;
  }
  // Go from right to left to findout if the route goes through the rightmost candidate anf fixes can be applied.
  // Other candidates which are sharper than kMaxAbsAngleConsideredLeftOrRightMost are ignored.
  for (auto candidate = turnCandidates.rbegin(); candidate != turnCandidates.rend(); ++candidate)
  {
    if (candidate->m_segment == firstOutgoingSeg && candidate + 1 != turnCandidates.rend())
    {
      // The route goes along the rightmost candidate.
      turn.m_turn = RightmostDirection(turnAngle);
      if (IntermediateDirection(turnAngle) != turn.m_turn)
        LOG(LDEBUG, ("Turn: ", turn.m_index, " RighmostDirection correction."));
      // Compare with the next candidate to the left.
      CorrectGoStraight(*(candidate + 1), candidate->m_angle, CarDirection::TurnSlightRight, turn);
      break;
    }
    // Check if this candidate is considered as rightmost as not too sharp.
    // If yes - this candidate is rightmost, not route's one.
    if (candidate->m_angle < kMaxAbsAngleConsideredLeftOrRightMost)
      break;
  }
}

void GetTurnDirectionBasic(IRoutingResult const & result, size_t const outgoingSegmentIndex,
                           NumMwmIds const & numMwmIds, RoutingSettings const & vehicleSettings,
                           TurnItem & turn)
{
  TurnInfo turnInfo;
  if (!GetTurnInfo(result, outgoingSegmentIndex, vehicleSettings, turnInfo))
    return;

  turn.m_turn = CarDirection::None;

  ASSERT_GREATER(turnInfo.m_ingoing->m_path.size(), 1, ());
  TurnCandidates nodes;
  size_t ingoingCount;
  m2::PointD const junctionPoint = turnInfo.m_ingoing->m_path.back().GetPoint();
  result.GetPossibleTurns(turnInfo.m_ingoing->m_segmentRange, junctionPoint, ingoingCount, nodes);
  if (nodes.isCandidatesAngleValid)
  {
    ASSERT(is_sorted(nodes.candidates.begin(), nodes.candidates.end(), base::LessBy(&TurnCandidate::m_angle)),
           ("Turn candidates should be sorted by its angle field."));
  }

  /// @todo Proper handling of isCandidatesAngleValid == False, when we don't have angles of candidates.

  if (nodes.candidates.size() == 0)
    return;

  // No turns are needed on transported road.
  if (turnInfo.m_ingoing->m_highwayClass == HighwayClass::Transported && turnInfo.m_outgoing->m_highwayClass == HighwayClass::Transported)
    return;

  Segment firstOutgoingSeg;
  if (!turnInfo.m_outgoing->m_segmentRange.GetFirstSegment(numMwmIds, firstOutgoingSeg))
    return;

  CorrectCandidatesSegmentByOutgoing(turnInfo, firstOutgoingSeg, nodes);

  RemoveUTurnCandidate(turnInfo, numMwmIds, nodes.candidates);
  auto const & turnCandidates = nodes.candidates;

  // Check for enter or exit to/from roundabout.
  if (turnInfo.m_ingoing->m_onRoundabout || turnInfo.m_outgoing->m_onRoundabout)
  {
    turn.m_turn = GetRoundaboutDirection(turnInfo, nodes, numMwmIds);
    // If there is 1 or more exits (nodes.candidates > 1) right at the enter it should be counted. Issue #3027.
    if (turn.m_turn == CarDirection::EnterRoundAbout && nodes.candidates.size() > 1)
      turn.m_exitNum = 1;
    return;
  }

  double const turnAngle = CalcTurnAngle(result, outgoingSegmentIndex, numMwmIds, vehicleSettings);

  CarDirection const intermediateDirection = IntermediateDirection(turnAngle);

  // Checking for exits from highways.
  turn.m_turn = TryToGetExitDirection(nodes, turnInfo, firstOutgoingSeg, intermediateDirection);
  if (turn.m_turn != CarDirection::None)
    return;

  if (turnCandidates.size() == 1)
  {
    //ASSERT_EQUAL(turnCandidates.front().m_segment, firstOutgoingSeg, ());

    if (IsGoStraightOrSlightTurn(intermediateDirection))
      return;
    // IngoingCount includes ingoing segment and reversed outgoing (if it is not one-way).
    // If any other one is present, turn (not straight or slight) is kept to prevent user from going to oneway alternative.

    /// @todo Min abs angle of ingoing ones should be considered. If it's much bigger than route angle - ignore ingoing ones.
    /// Now this data is not available from IRoutingResult::GetPossibleTurns().
    if (ingoingCount <= 1 + (turnInfo.m_outgoing->m_isOneWay ? 0 : 1))
      return;
  }

  // This angle is calculated using only 1 segment back and forward, not like turnAngle.
  double turnOneSegmentAngle = CalcOneSegmentTurnAngle(turnInfo);

  // To not discard some disputable turns let's use max by modulus from turnOneSegmentAngle and turnAngle.
  // It's natural since angles of turnCandidates are calculated in IRoutingResult::GetPossibleTurns()
  // according to CalcOneSegmentTurnAngle logic. And to be safe turnAngle is used too.
  double turnAngleToCompare = turnAngle;
  if (turnOneSegmentAngle <= 0 && turnAngle <= 0)
    turnAngleToCompare = min(turnOneSegmentAngle, turnAngle);
  else if (turnOneSegmentAngle >= 0 && turnAngle >= 0)
    turnAngleToCompare = max(turnOneSegmentAngle, turnAngle);
  else if (abs(turnOneSegmentAngle) > 10)
    LOG(LWARNING, ("Significant angles are expected to have the same sign."));

  if (CanDiscardTurnByHighwayClassOrAngles(intermediateDirection, turnAngleToCompare, turnCandidates, turnInfo, numMwmIds))
    return;

  turn.m_turn = intermediateDirection;

  if (turnCandidates.size() >= 2 && nodes.isCandidatesAngleValid)
    CorrectRightmostAndLeftmost(turnCandidates, firstOutgoingSeg, turnAngle, turn);
}

size_t CheckUTurnOnRoute(IRoutingResult const & result, size_t const outgoingSegmentIndex,
                         NumMwmIds const & numMwmIds, RoutingSettings const & vehicleSettings,
                         TurnItem & turn)
{
  size_t constexpr kUTurnLookAhead = 3;
  double constexpr kUTurnHeadingSensitivity = math::pi / 10.0;
  auto const & segments = result.GetSegments();

  // In this function we process the turn between the previous and the current
  // segments. So we need a shift to get the previous segment.
  ASSERT_GREATER(segments.size(), 1, ());
  ASSERT_GREATER(outgoingSegmentIndex, 0, ());
  ASSERT_GREATER(segments.size(), outgoingSegmentIndex, ());
  auto const & masterSegment = segments[outgoingSegmentIndex - 1];
  if (masterSegment.m_path.size() < 2)
    return 0;

  // Roundabout is not the UTurn.
  if (masterSegment.m_onRoundabout)
    return 0;
  for (size_t i = 0; i < kUTurnLookAhead && i + outgoingSegmentIndex < segments.size(); ++i)
  {
    auto const & checkedSegment = segments[outgoingSegmentIndex + i];
    if (checkedSegment.m_path.size() < 2)
      return 0;

    if (checkedSegment.m_roadNameInfo.m_name == masterSegment.m_roadNameInfo.m_name &&
        checkedSegment.m_highwayClass == masterSegment.m_highwayClass &&
        checkedSegment.m_isLink == masterSegment.m_isLink && !checkedSegment.m_onRoundabout)
    {
      auto const & path = masterSegment.m_path;
      auto const & pointBeforeTurn = path[path.size() - 2];
      auto const & turnPoint = path[path.size() - 1];
      auto const & pointAfterTurn = checkedSegment.m_path[1];
      // Same segment UTurn case.
      if (i == 0)
      {
        // TODO Fix direction calculation.
        // Warning! We can not determine UTurn direction in single edge case. So we use UTurnLeft.
        // We decided to add driving rules (left-right sided driving) to mwm header.
        if (pointBeforeTurn == pointAfterTurn && turnPoint != pointBeforeTurn)
        {
          turn.m_turn = CarDirection::UTurnLeft;
          return 1;
        }
        // Wide UTurn must have link in it's middle.
        return 0;
      }

      // Avoid the UTurn on unnamed roads inside the rectangle based distinct.
      if (checkedSegment.m_roadNameInfo.m_name.empty())
        return 0;

      // Avoid returning to the same edge after uturn somewere else.
      if (pointBeforeTurn == pointAfterTurn)
        return 0;

      m2::PointD const v1 = turnPoint.GetPoint() - pointBeforeTurn.GetPoint();
      m2::PointD const v2 = pointAfterTurn.GetPoint() - checkedSegment.m_path[0].GetPoint();

      auto angle = ang::TwoVectorsAngle(m2::PointD::Zero(), v1, v2);

      if (!base::AlmostEqualAbs(angle, math::pi, kUTurnHeadingSensitivity))
        return 0;

      // Determine turn direction.
      m2::PointD const junctionPoint = masterSegment.m_path.back().GetPoint();

      m2::PointD const ingoingPoint = GetPointForTurn(
          result, outgoingSegmentIndex, numMwmIds, vehicleSettings.m_maxIngoingPointsCount,
          vehicleSettings.m_minIngoingDistMeters, false /* forward */);
      m2::PointD const outgoingPoint = GetPointForTurn(
          result, outgoingSegmentIndex, numMwmIds, vehicleSettings.m_maxOutgoingPointsCount,
          vehicleSettings.m_minOutgoingDistMeters, true /* forward */);

      if (PiMinusTwoVectorsAngle(junctionPoint, ingoingPoint, outgoingPoint) < 0)
        turn.m_turn = CarDirection::UTurnLeft;
      else
        turn.m_turn = CarDirection::UTurnRight;
      return i + 1;
    }
  }

  return 0;
}
}  // namespace routing
