#pragma once

#include "routing/segment.hpp"
#include "routing/lanes.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "indexer/feature_decl.hpp"

#include "geometry/point2d.hpp"

#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace routing
{
/// \brief Unique identification for a road edge between two junctions (joints). The identifier
/// is represented by an mwm id, a feature id, a range of segment ids [|m_startSegId|, |m_endSegId|],
/// a direction and type.
struct SegmentRange
{
  friend std::string DebugPrint(SegmentRange const & segmentRange);

  SegmentRange() = default;
  SegmentRange(FeatureID const & featureId, uint32_t startSegId, uint32_t endSegId, bool forward,
               m2::PointD const & start, m2::PointD const & end);
  bool operator==(SegmentRange const & rh) const;
  bool operator<(SegmentRange const & rh) const;
  void Clear();
  bool IsEmpty() const;
  FeatureID const & GetFeature() const;
  /// \returns true if the instance of SegmentRange is correct.
  bool IsCorrect() const;
  /// \brief Fills |segment| with the first segment of this SegmentRange.
  /// \returns true if |segment| is filled and false otherwise.
  bool GetFirstSegment(NumMwmIds const & numMwmIds, Segment & segment) const;
  bool GetLastSegment(NumMwmIds const & numMwmIds, Segment & segment) const;

private:
  bool GetSegmentBySegId(uint32_t segId, NumMwmIds const & numMwmIds, Segment & segment) const;

  FeatureID m_featureId;
  // Note. If SegmentRange represents two directional feature |m_endSegId| is greater
  // than |m_startSegId| if |m_forward| == true.
  uint32_t m_startSegId = 0;   // The first segment index of SegmentRange.
  uint32_t m_endSegId = 0;     // The last segment index of SegmentRange.
  bool m_forward = true;       // Segment direction in |m_featureId|.
  // Note. According to current implementation SegmentRange is filled based on instances of
  // Edge class in DirectionsEngine::GetSegmentRangeAndAdjacentEdges() method. In Edge class
  // to identify fake edges (part of real and completely fake) is used coordinates of beginning
  // and ending of the edge. To keep SegmentRange instances unique for unique edges
  // in case of fake edges it's necessary to have |m_start| and |m_end| fields below.
  // @TODO(bykoianko) It's necessary to get rid of |m_start| and |m_end|.
  // Fake edges in IndexGraph is identified by instances of Segment
  // with Segment::m_mwmId == kFakeNumMwmId. So instead of |m_featureId| field in this class
  // number mwm id field and feature id (uint32_t) should be used and |m_start| and |m_end|
  // should be removed. To do that classes IndexRoadGraph, CarDirectionsEngine,
  // PedestrianDirectionsEngine and other should be significant refactored.
  m2::PointD m_start;          // Coordinates of start of last Edge in SegmentRange.
  m2::PointD m_end;            // Coordinates of end of SegmentRange.
};

namespace turns
{
double constexpr kFeaturesNearTurnMeters = 3.0;

/*!
 * \warning The order of values below shall not be changed.
 * TurnRight(TurnLeft) must have a minimal value and
 * TurnSlightRight(TurnSlightLeft) must have a maximum value
 * \warning The values of TurnDirection shall be synchronized with values of TurnDirection enum in
 * java.
 */
enum class CarDirection
{
  None = 0,
  GoStraight,

  TurnRight,
  TurnSharpRight,
  TurnSlightRight,

  TurnLeft,
  TurnSharpLeft,
  TurnSlightLeft,

  UTurnLeft,
  UTurnRight,

  EnterRoundAbout,
  LeaveRoundAbout,
  StayOnRoundAbout,

  StartAtEndOfStreet,
  ReachedYourDestination,

  ExitHighwayToLeft,
  ExitHighwayToRight,

  Count  /**< This value is used for internals only. */
};

std::string DebugPrint(CarDirection const l);

/*!
 * \warning The values of PedestrianDirectionType shall be synchronized with values in java
 */
enum class PedestrianDirection
{
  None = 0,
  GoStraight,
  TurnRight,
  TurnLeft,
  ReachedYourDestination,
  Count /**< This value is used for internals only. */
};

std::string DebugPrint(PedestrianDirection const l);

struct TurnItem
{
  TurnItem()
      : m_index(std::numeric_limits<uint32_t>::max()),
        m_turn(CarDirection::None),
        m_exitNum(0),
        m_pedestrianTurn(PedestrianDirection::None)
  {
  }

  TurnItem(uint32_t idx, CarDirection t, uint32_t exitNum = 0)
      : m_index(idx), m_turn(t), m_exitNum(exitNum)
      , m_pedestrianTurn(PedestrianDirection::None)
  {
  }

  TurnItem(uint32_t idx, PedestrianDirection p)
      : m_index(idx), m_turn(CarDirection::None), m_exitNum(0)
      , m_pedestrianTurn(p)
  {
  }

  bool operator==(TurnItem const & rhs) const
  {
    return m_index == rhs.m_index && m_turn == rhs.m_turn && m_lanes == rhs.m_lanes &&
           m_exitNum == rhs.m_exitNum && m_pedestrianTurn == rhs.m_pedestrianTurn;
  }

  bool IsTurnReachedYourDestination() const
  {
    return m_turn == CarDirection::ReachedYourDestination ||
           m_pedestrianTurn == PedestrianDirection::ReachedYourDestination;
  }

  bool IsTurnNone() const
  {
    return m_turn == CarDirection::None && m_pedestrianTurn == PedestrianDirection::None;
  }

  uint32_t m_index;                         // Index of point on route polyline (Index of segment + 1).
  CarDirection m_turn = CarDirection::None; // The turn instruction of the TurnItem.
  lanes::LanesInfo m_lanes;                 // Lane information on the edge before the turn.
  uint32_t m_exitNum;                       // Number of exit on roundabout.
  /*!
   * \brief m_pedestrianTurn is type of corresponding direction for a pedestrian, or None
   * if there is no pedestrian specific direction
   */
  PedestrianDirection m_pedestrianTurn = PedestrianDirection::None;
};

std::string DebugPrint(TurnItem const & turnItem);

struct TurnItemDist
{
  TurnItemDist(TurnItem const & turnItem, double distMeters)
    : m_turnItem(turnItem), m_distMeters(distMeters)
  {
  }
  TurnItemDist() = default;

  TurnItem m_turnItem;
  double m_distMeters = 0.0;
};

std::string DebugPrint(TurnItemDist const & turnItemDist);

std::string GetTurnString(CarDirection turn);

bool IsLeftTurn(CarDirection t);
bool IsRightTurn(CarDirection t);
bool IsLeftOrRightTurn(CarDirection t);
bool IsTurnMadeFromLeft(CarDirection t);
bool IsTurnMadeFromRight(CarDirection t);
bool IsStayOnRoad(CarDirection t);
bool IsGoStraightOrSlightTurn(CarDirection t);

/*!
 * \returns pi minus angle from vector [junctionPoint, ingoingPoint]
 * to vector [junctionPoint, outgoingPoint]. A counterclockwise rotation.
 * Angle is in range [-pi, pi].
*/
double PiMinusTwoVectorsAngle(m2::PointD const & junctionPoint, m2::PointD const & ingoingPoint,
                              m2::PointD const & outgoingPoint);
}  // namespace turns
}  // namespace routing
