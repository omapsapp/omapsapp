#include "turns.hpp"

#include "geometry/angles.hpp"

#include "platform/country_file.hpp"

#include "base/internal/message.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace routing
{
using namespace routing::turns;
using namespace std;

namespace
{
array<pair<CarDirection, char const *>, static_cast<size_t>(CarDirection::Count)> const
    g_turnNames = {{{CarDirection::None, "None"},
                    {CarDirection::GoStraight, "GoStraight"},
                    {CarDirection::TurnRight, "TurnRight"},
                    {CarDirection::TurnSharpRight, "TurnSharpRight"},
                    {CarDirection::TurnSlightRight, "TurnSlightRight"},
                    {CarDirection::TurnLeft, "TurnLeft"},
                    {CarDirection::TurnSharpLeft, "TurnSharpLeft"},
                    {CarDirection::TurnSlightLeft, "TurnSlightLeft"},
                    {CarDirection::UTurnLeft, "UTurnLeft"},
                    {CarDirection::UTurnRight, "UTurnRight"},
                    {CarDirection::EnterRoundAbout, "EnterRoundAbout"},
                    {CarDirection::LeaveRoundAbout, "LeaveRoundAbout"},
                    {CarDirection::StayOnRoundAbout, "StayOnRoundAbout"},
                    {CarDirection::StartAtEndOfStreet, "StartAtEndOfStreet"},
                    {CarDirection::ReachedYourDestination, "ReachedYourDestination"},
                    {CarDirection::ExitHighwayToLeft, "ExitHighwayToLeft"},
                    {CarDirection::ExitHighwayToRight, "ExitHighwayToRight"}}};
static_assert(g_turnNames.size() == static_cast<size_t>(CarDirection::Count),
              "Check the size of g_turnNames");
}  // namespace

// SegmentRange -----------------------------------------------------------------------------------
SegmentRange::SegmentRange(FeatureID const & featureId, uint32_t startSegId, uint32_t endSegId,
                           bool forward, m2::PointD const & start, m2::PointD const & end)
  : m_featureId(featureId), m_startSegId(startSegId), m_endSegId(endSegId), m_forward(forward),
    m_start(start), m_end(end)
{
  if (m_startSegId != m_endSegId)
    CHECK_EQUAL(m_forward, m_startSegId < m_endSegId, (*this));
}

bool SegmentRange::operator==(SegmentRange const & rhs) const
{
  return m_featureId == rhs.m_featureId && m_startSegId == rhs.m_startSegId &&
         m_endSegId == rhs.m_endSegId && m_forward == rhs.m_forward && m_start == rhs.m_start &&
         m_end == rhs.m_end;
}

bool SegmentRange::operator<(SegmentRange const & rhs) const
{
  if (m_featureId != rhs.m_featureId)
    return m_featureId < rhs.m_featureId;

  if (m_startSegId != rhs.m_startSegId)
    return m_startSegId < rhs.m_startSegId;

  if (m_endSegId != rhs.m_endSegId)
    return m_endSegId < rhs.m_endSegId;

  if (m_forward != rhs.m_forward)
    return m_forward < rhs.m_forward;

  if (m_start != rhs.m_start)
    return m_start < rhs.m_start;

  return m_end < rhs.m_end;
}

void SegmentRange::Clear()
{
  m_featureId = FeatureID();
  m_startSegId = 0;
  m_endSegId = 0;
  m_forward = true;
  m_start = m2::PointD::Zero();
  m_end = m2::PointD::Zero();
}

bool SegmentRange::IsEmpty() const
{
  return !m_featureId.IsValid() && m_startSegId == 0 && m_endSegId == 0 && m_forward &&
         m_start == m2::PointD::Zero() && m_end == m2::PointD::Zero();
}

FeatureID const & SegmentRange::GetFeature() const
{
  return m_featureId;
}

bool SegmentRange::IsCorrect() const
{
  return (m_forward && m_startSegId <= m_endSegId) || (!m_forward && m_endSegId <= m_startSegId);
}

bool SegmentRange::GetFirstSegment(NumMwmIds const & numMwmIds, Segment & segment) const
{
  return GetSegmentBySegId(m_startSegId, numMwmIds, segment);
}

bool SegmentRange::GetLastSegment(NumMwmIds const & numMwmIds, Segment & segment) const
{
  return GetSegmentBySegId(m_endSegId, numMwmIds, segment);
}

bool SegmentRange::GetSegmentBySegId(uint32_t segId, NumMwmIds const & numMwmIds,
                                     Segment & segment) const
{
  if (!m_featureId.IsValid())
    return false;

  segment = Segment(numMwmIds.GetId(platform::CountryFile(m_featureId.GetMwmName())),
                    m_featureId.m_index, segId, m_forward);
  return true;
}

string DebugPrint(SegmentRange const & segmentRange)
{
  stringstream out;
  out << "SegmentRange "
      << "{ m_featureId = " << DebugPrint(segmentRange.m_featureId)
      << ", m_startSegId = " << segmentRange.m_startSegId
      << ", m_endSegId = " << segmentRange.m_endSegId
      << ", m_forward = " << segmentRange.m_forward
      << ", m_start = " << DebugPrint(segmentRange.m_start)
      << ", m_end = " << DebugPrint(segmentRange.m_end)
      << " }";
  return out.str();
}

namespace turns
{
string DebugPrint(TurnItem const & turnItem)
{
  stringstream out;
  out << "TurnItem "
      << "{ m_index = " << turnItem.m_index
      << ", m_turn = " << DebugPrint(turnItem.m_turn)
      << ", m_lanes = " << lanes::DebugPrint(turnItem.m_lanes)
      << ", m_exitNum = " << turnItem.m_exitNum
      << ", m_pedestrianDir = " << DebugPrint(turnItem.m_pedestrianTurn)
      << " }";
  return out.str();
}

string DebugPrint(TurnItemDist const & turnItemDist)
{
  stringstream out;
  out << "TurnItemDist "
      << "{ m_turnItem = " << DebugPrint(turnItemDist.m_turnItem)
      << ", m_distMeters = " << turnItemDist.m_distMeters
      << " }";
  return out.str();
}

string GetTurnString(CarDirection turn)
{
  for (auto const & p : g_turnNames)
  {
    if (p.first == turn)
      return p.second;
  }

  ASSERT(false, (static_cast<int>(turn)));
  return "unknown CarDirection";
}

bool IsLeftTurn(CarDirection t)
{
  return (t >= CarDirection::TurnLeft && t <= CarDirection::TurnSlightLeft);
}

bool IsRightTurn(CarDirection t)
{
  return (t >= CarDirection::TurnRight && t <= CarDirection::TurnSlightRight);
}

bool IsLeftOrRightTurn(CarDirection t)
{
  return IsLeftTurn(t) || IsRightTurn(t);
}

bool IsTurnMadeFromLeft(CarDirection t)
{
  return IsLeftTurn(t) || t == CarDirection::UTurnLeft || t == CarDirection::ExitHighwayToLeft;
}

bool IsTurnMadeFromRight(CarDirection t)
{
  return IsRightTurn(t) || t == CarDirection::UTurnRight || t == CarDirection::ExitHighwayToRight;
}

bool IsStayOnRoad(CarDirection t)
{
  return (t == CarDirection::GoStraight || t == CarDirection::StayOnRoundAbout);
}

bool IsGoStraightOrSlightTurn(CarDirection t)
{
  return (t == CarDirection::GoStraight || t == CarDirection::TurnSlightLeft ||
          t == CarDirection::TurnSlightRight);
}

string DebugPrint(CarDirection const turn)
{
  return GetTurnString(turn);
}

string DebugPrint(PedestrianDirection const l)
{
  switch (l)
  {
  case PedestrianDirection::None: return "None";
  case PedestrianDirection::GoStraight: return "GoStraight";
  case PedestrianDirection::TurnRight: return "TurnRight";
  case PedestrianDirection::TurnLeft: return "TurnLeft";
  case PedestrianDirection::ReachedYourDestination: return "ReachedYourDestination";
  case PedestrianDirection::Count:
    // PedestrianDirection::Count should be never used in the code, print it as unknown value
    // (it is added to cases list to suppress compiler warning).
    break;
  }

  ASSERT(false, (static_cast<int>(l)));
  return "unknown PedestrianDirection";
}

double PiMinusTwoVectorsAngle(m2::PointD const & junctionPoint, m2::PointD const & ingoingPoint,
                              m2::PointD const & outgoingPoint)
{
  return math::pi - ang::TwoVectorsAngle(junctionPoint, ingoingPoint, outgoingPoint);
}
}  // namespace turns
}  // namespace routing
