#include "routing/turns_generator_utils.hpp"
#include "platform/measurement_utils.hpp"
#include "geometry/mercator.hpp"
#include "routing/turns.hpp"

namespace routing
{
namespace turns
{

using namespace ftypes;

bool IsHighway(HighwayClass hwClass, bool isLink)
{
  return (hwClass == HighwayClass::Trunk || hwClass == HighwayClass::Primary) &&
         !isLink;
}

bool IsSmallRoad(HighwayClass hwClass)
{
  return hwClass == HighwayClass::LivingStreet ||
         hwClass == HighwayClass::Service || hwClass == HighwayClass::Pedestrian;
}

int CalcDiffRoadClasses(HighwayClass const left, HighwayClass const right)
{
  return static_cast<int>(left) - static_cast<int>(right);
}

template <class T>
T FindDirectionByAngle(std::vector<std::pair<double, T>> const & lowerBounds, double const angle)
{
  ASSERT_GREATER_OR_EQUAL(angle, -180., (angle));
  ASSERT_LESS_OR_EQUAL(angle, 180., (angle));
  ASSERT(!lowerBounds.empty(), ());
  ASSERT(is_sorted(lowerBounds.rbegin(), lowerBounds.rend(), base::LessBy(&std::pair<double, T>::first)),
         ());

  for (auto const & lower : lowerBounds)
  {
    if (angle >= lower.first)
      return lower.second;
  }

  ASSERT(false, ("The angle is not covered by the table. angle = ", angle));
  return T::None;
}

CarDirection InvertDirection(CarDirection const dir)
{
  switch (dir)
  {
    case CarDirection::TurnSlightRight:
      return CarDirection::TurnSlightLeft;
    case CarDirection::TurnRight:
      return CarDirection::TurnLeft;
    case CarDirection::TurnSharpRight:
      return CarDirection::TurnSharpLeft;
    case CarDirection::TurnSlightLeft:
      return CarDirection::TurnSlightRight;
    case CarDirection::TurnLeft:
      return CarDirection::TurnRight;
    case CarDirection::TurnSharpLeft:
      return CarDirection::TurnSharpRight;
    default:
      return dir;
  };
}

CarDirection RightmostDirection(double const angle)
{
  static std::vector<std::pair<double, CarDirection>> const kLowerBounds = {
      {145., CarDirection::TurnSharpRight},
      {50., CarDirection::TurnRight},
      {10., CarDirection::TurnSlightRight},
      // For sure it's incorrect to give directions TurnLeft or TurnSlighLeft if we need the rightmost turn.
      // So GoStraight direction is given even for needed sharp left turn when other turns on the left and are more sharp.
      // The reason: the rightmost turn is the most straight one here.
      {-180., CarDirection::GoStraight}};

  return FindDirectionByAngle(kLowerBounds, angle);
}

CarDirection LeftmostDirection(double const angle)
{
  return InvertDirection(RightmostDirection(-angle));
}

CarDirection IntermediateDirection(double const angle)
{
  static std::vector<std::pair<double, CarDirection>> const kLowerBounds = {
      {145., CarDirection::TurnSharpRight},
      {50., CarDirection::TurnRight},
      {10., CarDirection::TurnSlightRight},
      {-10., CarDirection::GoStraight},
      {-50., CarDirection::TurnSlightLeft},
      {-145., CarDirection::TurnLeft},
      {-180., CarDirection::TurnSharpLeft}};

  return FindDirectionByAngle(kLowerBounds, angle);
}

PedestrianDirection IntermediateDirectionPedestrian(double const angle)
{
  static std::vector<std::pair<double, PedestrianDirection>> const kLowerBounds = {
      {10.0, PedestrianDirection::TurnRight},
      {-10.0, PedestrianDirection::GoStraight},
      {-180.0, PedestrianDirection::TurnLeft}};

  return FindDirectionByAngle(kLowerBounds, angle);
}

double CalcEstimatedTimeToPass(double const distanceMeters, HighwayClass const highwayClass)
{
  double speedKmph = 0;
  switch (highwayClass)
  {
    case HighwayClass::Trunk:         speedKmph = 100.0; break;
    case HighwayClass::Primary:       speedKmph = 70.0; break;
    case HighwayClass::Secondary:     speedKmph = 70.0; break;
    case HighwayClass::Tertiary:      speedKmph = 50.0; break;
    case HighwayClass::LivingStreet:  speedKmph = 20.0; break;
    case HighwayClass::Service:       speedKmph = 10.0; break;
    case HighwayClass::Pedestrian:    speedKmph = 5.0; break;
    default:                          speedKmph = 50.0; break;
  }
  return distanceMeters / measurement_utils::KmphToMps(speedKmph);
}

bool PathIsFakeLoop(std::vector<geometry::PointWithAltitude> const & path)
{
  return path.size() == 2 && path[0] == path[1];
}

double CalcRouteDistanceM(std::vector<geometry::PointWithAltitude> const & junctions, uint32_t start,
                          uint32_t end)
{
  double res = 0.0;

  for (uint32_t i = start + 1; i < end; ++i)
    res += mercator::DistanceOnEarth(junctions[i - 1].GetPoint(), junctions[i].GetPoint());

  return res;
}

// TurnInfo ----------------------------------------------------------------------------------------
bool TurnInfo::IsSegmentsValid() const
{
  if (m_ingoing->m_path.empty() || m_outgoing->m_path.empty())
  {
    LOG(LWARNING, ("Some turns can't load the geometry."));
    return false;
  }
  return true;
}

// RoutePointIndex ---------------------------------------------------------------------------------
bool RoutePointIndex::operator==(RoutePointIndex const & index) const
{
  return m_segmentIndex == index.m_segmentIndex && m_pathIndex == index.m_pathIndex;
}

std::string DebugPrint(RoutePointIndex const & index)
{
  std::stringstream out;
  out << "RoutePointIndex [ m_segmentIndex == " << index.m_segmentIndex
      << ", m_pathIndex == " << index.m_pathIndex << " ]" << std::endl;
  return out.str();
}

RoutePointIndex GetFirstOutgoingPointIndex(size_t outgoingSegmentIndex)
{
  return RoutePointIndex({outgoingSegmentIndex, 0 /* m_pathIndex */});
}

RoutePointIndex GetLastIngoingPointIndex(TUnpackedPathSegments const & segments,
                                         size_t const outgoingSegmentIndex)
{
  ASSERT_GREATER(outgoingSegmentIndex, 0, ());
  ASSERT(segments[outgoingSegmentIndex - 1].IsValid(), ());
  return RoutePointIndex({outgoingSegmentIndex - 1,
                          segments[outgoingSegmentIndex - 1].m_path.size() - 1 /* m_pathIndex */});
}

m2::PointD GetPointByIndex(TUnpackedPathSegments const & segments, RoutePointIndex const & index)
{
  return segments[index.m_segmentIndex].m_path[index.m_pathIndex].GetPoint();
}

double CalcOneSegmentTurnAngle(TurnInfo const & turnInfo)
{
  ASSERT_GREATER_OR_EQUAL(turnInfo.m_ingoing->m_path.size(), 2, ());
  ASSERT_GREATER_OR_EQUAL(turnInfo.m_outgoing->m_path.size(), 2, ());

  return math::RadToDeg(PiMinusTwoVectorsAngle(turnInfo.m_ingoing->m_path.back().GetPoint(),
                                               turnInfo.m_ingoing->m_path[turnInfo.m_ingoing->m_path.size() - 2].GetPoint(),
                                               turnInfo.m_outgoing->m_path[1].GetPoint()));
}

double CalcPathTurnAngle(LoadedPathSegment const & segment, size_t const pathIndex)
{
  ASSERT_GREATER_OR_EQUAL(segment.m_path.size(), 3, ());
  ASSERT_GREATER(pathIndex, 0, ());
  ASSERT_LESS(pathIndex, segment.m_path.size() - 1, ());

  return math::RadToDeg(PiMinusTwoVectorsAngle(segment.m_path[pathIndex].GetPoint(),
                                               segment.m_path[pathIndex - 1].GetPoint(),
                                               segment.m_path[pathIndex + 1].GetPoint()));
}

}  // namespace turns
}  // namespace routing
