#include "routing/turns_generator.hpp"

#include "routing/turns_generator_utils.hpp"

#include "indexer/ftypes_matcher.hpp"

#include "base/stl_helpers.hpp"

namespace routing
{
namespace turns
{
using namespace std;
using namespace ftypes;

/// * \returns true when
/// * - the route leads from one big road to another one;
/// * - and the other possible turns lead to small roads;
/// * Returns false otherwise.
/// \param turnCandidates is all possible ways out from a junction.
/// \param turnInfo is information about ingoing and outgoing segments of the route.
bool CanDiscardTurnByHighwayClass(std::vector<TurnCandidate> const & turnCandidates,
                                  TurnInfo const & turnInfo,
                                  NumMwmIds const & numMwmIds)
{
  HighwayClass outgoingRouteRoadClass = turnInfo.m_outgoing->m_highwayClass;
  HighwayClass ingoingRouteRoadClass = turnInfo.m_ingoing->m_highwayClass;

  HighwayClass maxRouteRoadClass = static_cast<HighwayClass>(max(static_cast<int>(ingoingRouteRoadClass), static_cast<int>(outgoingRouteRoadClass)));

  // The turn should be kept if there's no any information about feature id of outgoing segment
  // just to be on the safe side. It may happen in case of outgoing segment is a finish segment.
  Segment firstOutgoingSegment;
  if (!turnInfo.m_outgoing->m_segmentRange.GetFirstSegment(numMwmIds, firstOutgoingSegment))
    return false;

  for (auto const & t : turnCandidates)
  {
    // Let's consider all outgoing segments except for route outgoing segment.
    if (t.m_segment == firstOutgoingSegment)
      continue;

    HighwayClass candidateRoadClass = t.m_highwayClass;

    // If route's road is significantly larger than candidate's road, the candidate can be ignored.
    if (CalcDiffRoadClasses(maxRouteRoadClass, candidateRoadClass) <= kMinHighwayClassDiff)
      continue;

    // If route's road is significantly larger than candidate's service road, the candidate can be ignored.
    if (CalcDiffRoadClasses(maxRouteRoadClass, candidateRoadClass) <= kMinHighwayClassDiffForService && candidateRoadClass == HighwayClass::Service)
      continue;

    return false;
  }

  return true;
}

m2::PointD GetPointForTurn(IRoutingResult const & result, size_t outgoingSegmentIndex,
                           NumMwmIds const & numMwmIds, size_t const maxPointsCount,
                           double const maxDistMeters, bool const forward)
{
  auto const & segments = result.GetSegments();
  ASSERT_LESS(outgoingSegmentIndex, segments.size(), ());
  ASSERT_GREATER(outgoingSegmentIndex, 0, ());

  RoutePointIndex index = forward ? GetFirstOutgoingPointIndex(outgoingSegmentIndex)
                                  : GetLastIngoingPointIndex(segments, outgoingSegmentIndex);

  ASSERT_LESS(index.m_pathIndex, segments[index.m_segmentIndex].m_path.size(), ());
  ASSERT_LESS(index.m_segmentIndex, segments.size(), ());
  ASSERT(!segments[index.m_segmentIndex].m_path.empty(), ());

  RoutePointIndex nextIndex;
  ASSERT(GetNextRoutePointIndex(result, index, numMwmIds, forward, nextIndex), ());

  // There is no need for looking too far for low-speed roads.
  // So additional time limit is applied.
  double constexpr kMaxTimeSeconds = 3.0;

  m2::PointD point = GetPointByIndex(segments, index);

  size_t count = 0;
  double curDistanceMeters = 0.0;
  double curTimeSeconds = 0.0;

  while (GetNextRoutePointIndex(result, index, numMwmIds, forward, nextIndex))
  {
    m2::PointD nextPoint = GetPointByIndex(segments, nextIndex);

    // At start and finish there are two edges with zero length.
    // This function should not be called for the start (|outgoingSegmentIndex| == 0).
    // So there is special processing for the finish below.
    if (point == nextPoint && outgoingSegmentIndex + 1 == segments.size())
      return nextPoint;

    double distanceMeters = mercator::DistanceOnEarth(point, nextPoint);
    curDistanceMeters += distanceMeters;
    curTimeSeconds += CalcEstimatedTimeToPass(distanceMeters, segments[nextIndex.m_segmentIndex].m_highwayClass);

    if (curTimeSeconds > kMaxTimeSeconds || ++count >= maxPointsCount || curDistanceMeters > maxDistMeters)
      return nextPoint;

    point = nextPoint;
    index = nextIndex;
  }

  return point;
}

/*!
 * \brief Calculates |nextIndex| which is an index of next route point at result.GetSegments()
 * in forward direction.
 * If
 *  - |index| points at the last point of the turn segment:
 *  - and the route at this point leads from one big road to another one
 *  - and the other possible turns lead to small roads or there's no them
 *  - and the turn is GoStraight or TurnSlight*
 *  method returns the second point of the next segment. First point of the next segment is skipped
 *  because it's almost the same with the last point of this segment.
 *  if |index| points at the first or intermediate point in turn segment returns the next one.
 * \returns true if |nextIndex| fills correctly and false otherwise.
 */
bool GetNextCrossSegmentRoutePoint(IRoutingResult const & result, RoutePointIndex const & index,
                                   NumMwmIds const & numMwmIds, RoutePointIndex & nextIndex)
{
  auto const & segments = result.GetSegments();
  ASSERT_LESS(index.m_segmentIndex, segments.size(), ());
  ASSERT_LESS(index.m_pathIndex, segments[index.m_segmentIndex].m_path.size(), ());

  if (index.m_pathIndex + 1 != segments[index.m_segmentIndex].m_path.size())
  {
    // In segment case.
    nextIndex = {index.m_segmentIndex, index.m_pathIndex + 1};
    return true;
  }

  // Case when the last point of the current segment is reached.
  // So probably it's necessary to cross a segment border.
  if (index.m_segmentIndex + 1 == segments.size())
    return false; // The end of the route is reached.

  TurnInfo const turnInfo(&segments[index.m_segmentIndex], &segments[index.m_segmentIndex + 1]);

  double const oneSegmentTurnAngle = CalcOneSegmentTurnAngle(turnInfo);
  CarDirection const oneSegmentDirection = IntermediateDirection(oneSegmentTurnAngle);
  if (!IsGoStraightOrSlightTurn(oneSegmentDirection))
    return false; // Too sharp turn angle.

  size_t ingoingCount = 0;
  TurnCandidates possibleTurns;
  result.GetPossibleTurns(turnInfo.m_ingoing->m_segmentRange, GetPointByIndex(segments, index),
                          ingoingCount, possibleTurns);

  if (possibleTurns.candidates.empty())
    return false;

  // |segments| is a vector of |LoadedPathSegment|. Every |LoadedPathSegment::m_path|
  // contains junctions of the segment. The first junction at a |LoadedPathSegment::m_path|
  // is the same (or almost the same) with the last junction at the next |LoadedPathSegment::m_path|.
  // To prevent using the same point twice it's necessary to take the first point only from the
  // first item of |loadedSegments|. The beginning should be ignored for the rest |m_path|.
  // Please see a comment in MakeTurnAnnotation() for more details.
  if (possibleTurns.candidates.size() == 1)
  {
    // Taking the next point of the next segment.
    nextIndex = {index.m_segmentIndex + 1, 1 /* m_pathIndex */};
    return true;
  }

  if (CanDiscardTurnByHighwayClass(possibleTurns.candidates, turnInfo, numMwmIds))
  {
    // Taking the next point of the next segment.
    nextIndex = {index.m_segmentIndex + 1, 1 /* m_pathIndex */};
    return true;
  }
  // Stopping getting next route points because an important bifurcation point is reached.
  return false;
}

bool GetPrevInSegmentRoutePoint(IRoutingResult const & result, RoutePointIndex const & index, RoutePointIndex & nextIndex)
{
  if (index.m_pathIndex == 0)
    return false;

  auto const & segments = result.GetSegments();
  if (segments[index.m_segmentIndex].m_path.size() >= 3 && index.m_pathIndex < segments[index.m_segmentIndex].m_path.size() - 1)
  {
    double const oneSegmentTurnAngle = CalcPathTurnAngle(segments[index.m_segmentIndex], index.m_pathIndex);
    CarDirection const oneSegmentDirection = IntermediateDirection(oneSegmentTurnAngle);
    if (!IsGoStraightOrSlightTurn(oneSegmentDirection))
      return false; // Too sharp turn angle.
  }

  nextIndex = {index.m_segmentIndex, index.m_pathIndex - 1};
  return true;
}

bool GetNextRoutePointIndex(IRoutingResult const & result, RoutePointIndex const & index,
                            NumMwmIds const & numMwmIds, bool const forward,
                            RoutePointIndex & nextIndex)
{
  if (forward)
  {
    if (!GetNextCrossSegmentRoutePoint(result, index, numMwmIds, nextIndex))
      return false;
  }
  else
  {
    if (!GetPrevInSegmentRoutePoint(result, index, nextIndex))
      return false;
  }

  ASSERT_LESS(nextIndex.m_segmentIndex, result.GetSegments().size(), ());
  ASSERT_LESS(nextIndex.m_pathIndex, result.GetSegments()[nextIndex.m_segmentIndex].m_path.size(), ());
  return true;
}

void RemoveUTurnCandidate(TurnInfo const & turnInfo, NumMwmIds const & numMwmIds, std::vector<TurnCandidate> & turnCandidates)
{
  Segment lastIngoingSegment;
  if (turnInfo.m_ingoing->m_segmentRange.GetLastSegment(numMwmIds, lastIngoingSegment))
  {
    if (turnCandidates.front().m_segment.IsInverse(lastIngoingSegment))
      turnCandidates.erase(turnCandidates.begin());
    else if (turnCandidates.back().m_segment.IsInverse(lastIngoingSegment))
      turnCandidates.pop_back();
  }
}

/// \returns true if there is exactly 1 turn in |turnCandidates| with angle less then
/// |kMaxForwardAngleCandidates|.
bool HasSingleForwardTurn(TurnCandidates const & turnCandidates, float maxForwardAngleCandidates)
{
  bool foundForwardTurn = false;

  for (auto const & turn : turnCandidates.candidates)
  {
    if (std::fabs(turn.m_angle) < maxForwardAngleCandidates)
    {
      if (foundForwardTurn)
        return false;

      foundForwardTurn = true;
    }
  }

  return foundForwardTurn;
}

double CalcTurnAngle(IRoutingResult const & result,
                     size_t const outgoingSegmentIndex,
                     NumMwmIds const & numMwmIds,
                     RoutingSettings const & vehicleSettings)
{
  bool const isStartFakeLoop = PathIsFakeLoop(result.GetSegments()[outgoingSegmentIndex - 1].m_path);
  size_t const segmentIndexForIngoingPoint = isStartFakeLoop ? outgoingSegmentIndex - 1 : outgoingSegmentIndex;

  m2::PointD const ingoingPoint = GetPointForTurn(
      result, segmentIndexForIngoingPoint, numMwmIds, vehicleSettings.m_maxIngoingPointsCount,
      vehicleSettings.m_minIngoingDistMeters, false /* forward */);
  m2::PointD const outgoingPoint = GetPointForTurn(
      result, outgoingSegmentIndex, numMwmIds, vehicleSettings.m_maxOutgoingPointsCount,
      vehicleSettings.m_minOutgoingDistMeters, true /* forward */);

  m2::PointD const junctionPoint = result.GetSegments()[outgoingSegmentIndex].m_path.front().GetPoint();
  return math::RadToDeg(PiMinusTwoVectorsAngle(junctionPoint, ingoingPoint, outgoingPoint));
}

void CorrectCandidatesSegmentByOutgoing(TurnInfo const & turnInfo, Segment const & firstOutgoingSeg,
                                        TurnCandidates & nodes)
{
  double const turnAngle = CalcOneSegmentTurnAngle(turnInfo);
  auto const IsFirstOutgoingSeg = [&firstOutgoingSeg](TurnCandidate const & turnCandidate)
  {
    return turnCandidate.m_segment == firstOutgoingSeg;
  };
  auto & candidates = nodes.candidates;
  auto it = find_if(candidates.begin(), candidates.end(), IsFirstOutgoingSeg);
  if (it == candidates.end())
  {
    // firstOutgoingSeg not found. Try to match by angle.
    auto const DoesAngleMatch = [&turnAngle](TurnCandidate const & candidate)
    {
      return (AlmostEqualAbs(candidate.m_angle, turnAngle, 0.001) ||
              fabs(candidate.m_angle) + fabs(turnAngle) > 359.999);
    };
    auto it = find_if(candidates.begin(), candidates.end(), DoesAngleMatch);
    if (it != candidates.end())
    {
      // Match by angle. Update candidate's segment.
      ASSERT(it->m_segment.GetMwmId() != firstOutgoingSeg.GetMwmId(), ());
      ASSERT(it->m_segment.GetSegmentIdx() == firstOutgoingSeg.GetSegmentIdx(), ());
      ASSERT(it->m_segment.IsForward() == firstOutgoingSeg.IsForward(), ());
      it->m_segment = firstOutgoingSeg;
    }
    else if (nodes.isCandidatesAngleValid)
    {
      // Typically all candidates are from one mwm, and missed one (firstOutgoingSegment) from another.
      LOG(LWARNING, ("Can't match any candidate with firstOutgoingSegment but isCandidatesAngleValid == true."));
    }
    else
    {
      LOG(LWARNING, ("Can't match any candidate with firstOutgoingSegment and isCandidatesAngleValid == false"));
      if (candidates.size() == 1)
      {
        auto & c = candidates.front();
        ASSERT(c.m_segment.GetMwmId() != firstOutgoingSeg.GetMwmId(), ());
        ASSERT(c.m_segment.GetSegmentIdx() == firstOutgoingSeg.GetSegmentIdx(), ());
        ASSERT(c.m_segment.IsForward() == firstOutgoingSeg.IsForward(), ());
        c.m_segment = firstOutgoingSeg;
        c.m_angle = turnAngle;
        nodes.isCandidatesAngleValid = true;
        LOG(LWARNING, ("but since candidates.size() == 1, this was fixed."));
      }
      else
        LOG(LWARNING, ("and since candidates.size() > 1, this can't be fixed."));
    }
  }
  else // firstOutgoingSeg is found.
  {
    if (nodes.isCandidatesAngleValid)
    {
#ifdef DEBUG
      double constexpr eps = 0.001;
      double const diff = fabs(it->m_angle - turnAngle);
      ASSERT(diff < eps || fabs(diff - 360) < eps, (it->m_angle, turnAngle));
#endif
    }
    else
    {
      it->m_angle = turnAngle;
      if (candidates.size() == 1)
        nodes.isCandidatesAngleValid = true;
      else
        LOG(LWARNING, ("isCandidatesAngleValid == false, and this can't be fixed."));
    }
  }
}

bool GetTurnInfo(IRoutingResult const & result, size_t const outgoingSegmentIndex,
                 RoutingSettings const & vehicleSettings,
                 TurnInfo & turnInfo)
{
  auto const & segments = result.GetSegments();
  CHECK_LESS(outgoingSegmentIndex, segments.size(), ());
  CHECK_GREATER(outgoingSegmentIndex, 0, ());

  if (PathIsFakeLoop(segments[outgoingSegmentIndex].m_path))
    return false;

  bool const isStartFakeLoop = PathIsFakeLoop(segments[outgoingSegmentIndex - 1].m_path);

  if (isStartFakeLoop && outgoingSegmentIndex < 2)
    return false;

  size_t const prevIndex = isStartFakeLoop ? outgoingSegmentIndex - 2 : outgoingSegmentIndex - 1;
  turnInfo = TurnInfo(&segments[prevIndex], &segments[outgoingSegmentIndex]);

  if (!turnInfo.IsSegmentsValid() || turnInfo.m_ingoing->m_segmentRange.IsEmpty())
    return false;

  if (isStartFakeLoop)
  {
    if (mercator::DistanceOnEarth(turnInfo.m_ingoing->m_path.front().GetPoint(),
                                  turnInfo.m_ingoing->m_path.back().GetPoint()) < vehicleSettings.m_minIngoingDistMeters ||
        mercator::DistanceOnEarth(turnInfo.m_outgoing->m_path.front().GetPoint(),
                                  turnInfo.m_outgoing->m_path.back().GetPoint()) < vehicleSettings.m_minOutgoingDistMeters)
    {
      return false;
    }
  }

  ASSERT(!turnInfo.m_ingoing->m_path.empty(), ());
  ASSERT(!turnInfo.m_outgoing->m_path.empty(), ());
  ASSERT_LESS(mercator::DistanceOnEarth(turnInfo.m_ingoing->m_path.back().GetPoint(),
                                        turnInfo.m_outgoing->m_path.front().GetPoint()),
              kFeaturesNearTurnMeters, ());

  return true;
}

}  // namespace turns
}  // namespace routing
