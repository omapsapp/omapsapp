#pragma once

#include "routing/routing_options.hpp"
#include "routing/routing_settings.hpp"
#include "routing/segment.hpp"
#include "routing/transit_info.hpp"
#include "routing/turns.hpp"

#include "routing/base/followed_polyline.hpp"

#include "routing_common/maxspeed_conversion.hpp"

#include "traffic/speed_groups.hpp"

#include "platform/country_file.hpp"

#include "geometry/point_with_altitude.hpp"
#include "geometry/polyline2d.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"

#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace location
{
  class GpsInfo;
  class RouteMatchingInfo;
}

namespace routing
{
using SubrouteUid = uint64_t;
SubrouteUid constexpr kInvalidSubrouteId = std::numeric_limits<uint64_t>::max();

/// \brief The route is composed of one or several subroutes. Every subroute is composed of segments.
/// For every Segment is kept some attributes in the structure SegmentInfo.
class RouteSegment final
{
public:
  struct SpeedCamera
  {
    SpeedCamera() = default;
    SpeedCamera(double coef, uint8_t maxSpeedKmPH): m_coef(coef), m_maxSpeedKmPH(maxSpeedKmPH) {}

    bool EqualCoef(SpeedCamera const & rhs) const
    {
      return AlmostEqualAbs(m_coef, rhs.m_coef, 1.0E-5);
    }

    bool operator<(SpeedCamera const & rhs) const
    {
      if (!EqualCoef(rhs))
        return m_coef < rhs.m_coef;

      // Keep a camera with lowest speed (unique).
      return m_maxSpeedKmPH < rhs.m_maxSpeedKmPH;
    }

    friend std::string DebugPrint(SpeedCamera const & rhs);

    /// @todo Can replace with uint16_t feature node index, assuming that all cameras are placed on nodes.
    // Сoefficient where camera placed at the segment (number from 0 to 1).
    double m_coef = 0.0;
    // Max speed
    uint8_t m_maxSpeedKmPH = 0;
  };

  struct RoadNameInfo
  {
    // This is for street/road. |m_ref| |m_name|.
    std::string m_name;             // E.g "Johnson Ave.".
    std::string m_destination_ref;  // Number of next road, e.g. "CA 85", Sometimes "CA 85 South". Usually match |m_ref|
                                    // of next main road.
    // This is for 1st segment of link after junction. Exit |junction_ref| to |m_destination_ref| for |m_destination|.
    std::string m_junction_ref;     // Number of junction e.g. "398B".
    std::string m_destination;      // E.g. "Cupertino".
    std::string m_ref;              // Number of street/road e.g. "CA 85".
    bool m_isLink = false;

    RoadNameInfo() = default;
    RoadNameInfo(std::string name) : m_name(std::move(name)) {}
    RoadNameInfo(std::string name, std::string destination_ref)
      : m_name(std::move(name)), m_destination_ref(std::move(destination_ref))
    {
    }
    RoadNameInfo(std::string name, std::string destination_ref, std::string junction_ref)
      : m_name(std::move(name)), m_destination_ref(std::move(destination_ref)), m_junction_ref(std::move(junction_ref))
    {
    }
    RoadNameInfo(std::string name, std::string ref, std::string junction_ref, std::string destination_ref,
                 std::string destination, bool isLink)
      : m_name(std::move(name))
      , m_destination_ref(std::move(destination_ref))
      , m_junction_ref(std::move(junction_ref))
      , m_destination(std::move(destination))
      , m_ref(std::move(ref))
      , m_isLink(std::move(isLink))
    {
    }

    bool HasBasicTextInfo() const { return !m_ref.empty() || !m_name.empty(); }
    bool HasExitInfo() const { return m_isLink || HasExitTextInfo(); }
    bool HasExitTextInfo() const
    {
      return !m_junction_ref.empty() || !m_destination_ref.empty() || !m_destination.empty();
    }
    bool empty() const
    {
      return m_name.empty() && m_ref.empty() && m_junction_ref.empty() && m_destination_ref.empty() &&
             m_destination.empty();
    }

    bool operator==(RoadNameInfo const & rni) const
    {
      return m_name == rni.m_name && m_ref == rni.m_ref && m_junction_ref == rni.m_junction_ref &&
             m_destination_ref == rni.m_destination_ref && m_destination == rni.m_destination;
    }

    friend std::string DebugPrint(RoadNameInfo const & rni);
  };

  RouteSegment(Segment const & segment, turns::TurnItem const & turn,
               geometry::PointWithAltitude const & junction, RoadNameInfo const & roadNameInfo)
    : m_segment(segment)
    , m_turn(turn)
    , m_junction(junction)
    , m_roadNameInfo(roadNameInfo)
    , m_transitInfo(nullptr)
  {
  }

  void ClearTurn()
  {
    m_turn.m_turn = turns::CarDirection::None;
    m_turn.m_pedestrianTurn = turns::PedestrianDirection::None;
  }

  void SetTurnExits(uint32_t exitNum) { m_turn.m_exitNum = exitNum; }

  std::vector<turns::SingleLaneInfo> & GetTurnLanes() { return m_turn.m_lanes; };

  void SetDistancesAndTime(double distFromBeginningMeters, double distFromBeginningMerc, double timeFromBeginningS)
  {
    m_distFromBeginningMeters = distFromBeginningMeters;
    m_distFromBeginningMerc = distFromBeginningMerc;
    m_timeFromBeginningS = timeFromBeginningS;
  }

  void SetTransitInfo(std::unique_ptr<TransitInfo> transitInfo)
  {
    m_transitInfo.Set(std::move(transitInfo));
  }

  Segment const & GetSegment() const { return m_segment; }
  Segment & GetSegment() { return m_segment; }
  geometry::PointWithAltitude const & GetJunction() const { return m_junction; }
  RoadNameInfo const & GetRoadNameInfo() const { return m_roadNameInfo; }
  turns::TurnItem const & GetTurn() const { return m_turn; }
  void ClearTurnLanes() { m_turn.m_lanes.clear(); }

  double GetDistFromBeginningMeters() const { return m_distFromBeginningMeters; }
  double GetDistFromBeginningMerc() const { return m_distFromBeginningMerc; }
  double GetTimeFromBeginningSec() const { return m_timeFromBeginningS; }

  bool HasTransitInfo() const { return m_transitInfo.HasTransitInfo(); }
  TransitInfo const & GetTransitInfo() const { return m_transitInfo.Get(); }

  void SetSpeedCameraInfo(std::vector<SpeedCamera> && data) { m_speedCameras = std::move(data); }
  std::vector<SpeedCamera> const & GetSpeedCams() const { return m_speedCameras; }

  RoutingOptions GetRoadTypes() const { return m_roadTypes; }
  void SetRoadTypes(RoutingOptions types) { m_roadTypes = types; }

  traffic::SpeedGroup GetTraffic() const { return m_traffic; }
  void SetTraffic(traffic::SpeedGroup group) { m_traffic = group; }

  SpeedInUnits const & GetSpeedLimit() const { return m_speedLimit; }
  void SetSpeedLimit(SpeedInUnits const & speed) { m_speedLimit = speed; }

private:
  Segment m_segment;

  /// Turn (maneuver) information for the turn next to the |m_segment| if any.
  /// |m_turn::m_index| == segment index + 1.
  /// If not |m_turn::m_turn| is equal to TurnDirection::None.
  turns::TurnItem m_turn;

  /// The furthest point of the segment from the beginning of the route along the route.
  geometry::PointWithAltitude m_junction;

  /// RoadNameInfo of |m_segment| if any. Otherwise |m_roadNameInfo| is empty.
  RoadNameInfo m_roadNameInfo;

  /// Speed limit of |m_segment| if any.
  SpeedInUnits m_speedLimit;

  /// Distance from the route (not the subroute) beginning to the farthest end of |m_segment| in meters.
  double m_distFromBeginningMeters = 0.0;

  /// Distance from the route (not the subroute) beginning to the farthest end of |m_segment| in mercator.
  double m_distFromBeginningMerc = 0.0;

  /// ETA from the route beginning (not the subroute) in seconds to reach the farthest from the route beginning
  /// end of |m_segment|.
  double m_timeFromBeginningS = 0.0;

  traffic::SpeedGroup m_traffic = traffic::SpeedGroup::Unknown;

  /// Information needed to display transit segments properly.
  TransitInfoWrapper m_transitInfo;

  // List of speed cameras, sorted by segment direction (from start to end).
  // Stored coefficients where they placed at the segment (numbers from 0 to 1)
  // and theirs' max speed.
  std::vector<SpeedCamera> m_speedCameras;

  RoutingOptions m_roadTypes;
};

class Route
{
public:
  class SubrouteAttrs final
  {
  public:
    SubrouteAttrs() = default;

    SubrouteAttrs(geometry::PointWithAltitude const & start,
                  geometry::PointWithAltitude const & finish, size_t beginSegmentIdx,
                  size_t endSegmentIdx)
      : m_start(start)
      , m_finish(finish)
      , m_beginSegmentIdx(beginSegmentIdx)
      , m_endSegmentIdx(endSegmentIdx)
    {
      CHECK_LESS_OR_EQUAL(beginSegmentIdx, endSegmentIdx, ());
    }

    SubrouteAttrs(SubrouteAttrs const & subroute, size_t beginSegmentIdx)
      : m_start(subroute.m_start)
      , m_finish(subroute.m_finish)
      , m_beginSegmentIdx(beginSegmentIdx)
      , m_endSegmentIdx(beginSegmentIdx + subroute.GetSize())
    {
    }

    geometry::PointWithAltitude const & GetStart() const { return m_start; }
    geometry::PointWithAltitude const & GetFinish() const { return m_finish; }

    size_t GetBeginSegmentIdx() const { return m_beginSegmentIdx; }
    size_t GetEndSegmentIdx() const { return m_endSegmentIdx; }

    size_t GetSize() const { return m_endSegmentIdx - m_beginSegmentIdx; }

  private:
    geometry::PointWithAltitude m_start;
    geometry::PointWithAltitude m_finish;

    // Index of the first subroute segment in the whole route.
    size_t m_beginSegmentIdx = 0;

    // Non inclusive index of the last subroute segment in the whole route.
    size_t m_endSegmentIdx = 0;
  };

  /// \brief For every subroute some attributes are kept in the following structure.
  struct SubrouteSettings final
  {
    SubrouteSettings(RoutingSettings const & routingSettings, std::string const & router,
                     SubrouteUid id)
      : m_routingSettings(routingSettings), m_router(router), m_id(id)
    {
    }

    RoutingSettings const m_routingSettings;
    std::string const m_router;
    /// Some subsystems (for example drape) which is used Route class need to have an id of any subroute.
    /// This subsystems may set the id and then use it. The id is kept in |m_id|.
    SubrouteUid const m_id = kInvalidSubrouteId;
  };

  Route(std::string const & router, uint64_t routeId)
    : m_router(router), m_routingSettings(GetRoutingSettings(VehicleType::Car)), m_routeId(routeId)
  {
  }

  template <class TIter>
  Route(std::string const & router, TIter beg, TIter end, uint64_t routeId)
    : m_router(router)
    , m_routingSettings(GetRoutingSettings(VehicleType::Car))
    , m_poly(beg, end)
    , m_routeId(routeId)
  {
  }

  Route(std::string const & router, std::vector<m2::PointD> const & points, uint64_t routeId,
        std::string const & name = std::string());

  template <class TIter> void SetGeometry(TIter beg, TIter end)
  {
    if (beg == end)
    {
      FollowedPolyline().Swap(m_poly);
    }
    else
    {
      FollowedPolyline(beg, end).Swap(m_poly);
      // If there are no intermediate points it's acceptable to have an empty m_subrouteAttrs.
      // Constructed m_poly will have the last point index as next checkpoint index, it's right.
      if (!m_subrouteAttrs.empty())
      {
        ASSERT_GREATER(m_subrouteAttrs.size(), m_currentSubrouteIdx, ());
        m_poly.SetNextCheckpointIndex(m_subrouteAttrs[m_currentSubrouteIdx].GetEndSegmentIdx());
      }
    }
  }

  void SetRouteSegments(std::vector<RouteSegment> && routeSegments);

  std::vector<RouteSegment> & GetRouteSegments() { return m_routeSegments; }
  std::vector<RouteSegment> const & GetRouteSegments() const { return m_routeSegments; }
  RoutingSettings const & GetCurrentRoutingSettings() const { return m_routingSettings; }

  void SetCurrentSubrouteIdx(size_t currentSubrouteIdx) { m_currentSubrouteIdx = currentSubrouteIdx; }

  template <class V>
  void SetSubroteAttrs(V && subroutes)
  {
    m_subrouteAttrs = std::forward<V>(subroutes);
    ASSERT_GREATER(m_subrouteAttrs.size(), m_currentSubrouteIdx, ());
    m_poly.SetNextCheckpointIndex(m_subrouteAttrs[m_currentSubrouteIdx].GetEndSegmentIdx());
  }

  void PassNextSubroute()
  {
    ASSERT_GREATER(m_subrouteAttrs.size(), m_currentSubrouteIdx, ());
    m_currentSubrouteIdx = std::min(m_currentSubrouteIdx + 1, m_subrouteAttrs.size() - 1);
    m_poly.SetNextCheckpointIndex(m_subrouteAttrs[m_currentSubrouteIdx].GetEndSegmentIdx());
  }

  /// \returns estimated time for the whole route.
  double GetTotalTimeSec() const;

  /// \returns estimated time to reach the route end.
  double GetCurrentTimeToEndSec() const;

  /// \brief estimated time to reach segment.
  double GetCurrentTimeToSegmentSec(size_t segIdx) const;

  /// \brief estimated time to the nearest turn.
  double GetCurrentTimeToNearestTurnSec() const;

  FollowedPolyline const & GetFollowedPolyline() const { return m_poly; }

  std::string const & GetRouterId() const { return m_router; }
  m2::PolylineD const & GetPoly() const { return m_poly.GetPolyline(); }

  size_t GetCurrentSubrouteIdx() const { return m_currentSubrouteIdx; }
  std::vector<SubrouteAttrs> const & GetSubroutes() const { return m_subrouteAttrs; }

  std::vector<double> const & GetSegDistanceMeters() const { return m_poly.GetSegDistanceMeters(); }
  bool IsValid() const { return m_poly.IsValid(); }

  double GetTotalDistanceMeters() const;
  double GetCurrentDistanceFromBeginMeters() const;
  double GetCurrentDistanceToEndMeters() const;
  double GetMercatorDistanceFromBegin() const;

  /// \brief Extracts information about the nearest turn from the remaining part of the route.
  /// \param distanceToTurnMeters is a distance from current position to the nearest turn.
  /// \param turn is information about the nearest turn.
  void GetNearestTurn(double & distanceToTurnMeters, turns::TurnItem & turn) const;

  /// \returns information about turn from RouteSegment according to current iterator
  /// set with MoveIterator() method. If it's not possible returns nullopt.
  std::optional<turns::TurnItem> GetCurrentIteratorTurn() const;

  /// \brief Returns first non-empty name info of a street starting from segIdx.
  void GetClosestStreetNameAfterIdx(size_t segIdx, RouteSegment::RoadNameInfo & roadNameInfo) const;

  /// \brief Returns name info of a street where the user rides at this moment.
  void GetCurrentStreetName(RouteSegment::RoadNameInfo & roadNameInfo) const;

  /// \brief Returns current speed limit
  void GetCurrentSpeedLimit(SpeedInUnits & speedLimit) const;

  /// \brief Return name info of a street according to the next turn.
  void GetNextTurnStreetName(RouteSegment::RoadNameInfo & roadNameInfo) const;

  /// \brief Return name info of a street according to the next next turn.
  void GetNextNextTurnStreetName(RouteSegment::RoadNameInfo & roadNameInfo) const;

  /// \brief Gets turn information after the turn next to the nearest one.
  /// \param distanceToTurnMeters is a distance from current position to the second turn.
  /// \param nextTurn is information about the second turn.
  /// \note All parameters are filled while a GetNextTurn function call.
  bool GetNextTurn(double & distanceToTurnMeters, turns::TurnItem & nextTurn) const;
  /// \brief Extract information about zero, one or two nearest turns depending on current position.
  bool GetNextTurns(std::vector<turns::TurnItemDist> & turns) const;

  void GetCurrentDirectionPoint(m2::PointD & pt) const;

  bool MoveIterator(location::GpsInfo const & info);

  /// \brief Finds projection of |location| to the nearest route and sets |routeMatchingInfo|.
  /// fields accordingly.
  bool MatchLocationToRoute(location::GpsInfo & location,
                            location::RouteMatchingInfo & routeMatchingInfo) const;

  /// Add country name if we have no country filename to make route.
  void AddAbsentCountry(std::string const & name);

  /// Get absent file list of a routing files for shortest path finding.
  std::set<std::string> const & GetAbsentCountries() const { return m_absentCountries; }

  inline void SetRoutingSettings(RoutingSettings const & routingSettings)
  {
    m_routingSettings = routingSettings;
  }

  // Subroute interface.
  /// \returns Number of subroutes.
  /// \note Intermediate points separate a route into several subroutes.
  size_t GetSubrouteCount() const;

  /// \brief Fills |info| with full subroute information.
  /// \param subrouteIdx zero base number of subroute. |segmentIdx| should be less than GetSubrouteCount();
  /// \note |info| is a segment oriented route. Size of |info| is equal to number of points in |m_poly| - 1.
  /// Class Route is a point oriented route. While this conversion some attributes of zero point will be lost.
  /// It happens with zero turn for example.
  /// \note It's a fake implementation for single subroute which is equal to route without any
  /// intermediate points.
  /// Note. SegmentInfo::m_segment is filled with default Segment instance.
  /// Note. SegmentInfo::m_streetName is filled with an empty string.
  void GetSubrouteInfo(size_t subrouteIdx, std::vector<RouteSegment> & segments) const;

  SubrouteAttrs const & GetSubrouteAttrs(size_t subrouteIdx) const;

  /// \returns Subroute settings by |segmentIdx|.
  // @TODO(bykoianko) This method should return SubrouteSettings by reference. Now it returns by value
  // because of fake implementation.
  SubrouteSettings const GetSubrouteSettings(size_t segmentIdx) const;

  bool IsSubroutePassed(size_t subrouteIdx) const;

  /// \brief Sets subroute unique id (|subrouteUid|) by |segmentIdx|.
  /// \note |subrouteUid| is a permanent id of a subroute. This id can be used to address to a subroute
  /// after the route is removed.
  void SetSubrouteUid(size_t segmentIdx, SubrouteUid subrouteUid);

  void GetAltitudes(geometry::Altitudes & altitudes) const;
  bool HaveAltitudes() const { return m_haveAltitudes; }
  traffic::SpeedGroup GetTraffic(size_t segmentIdx) const;

  void GetTurnsForTesting(std::vector<turns::TurnItem> & turns) const;
  bool IsRouteId(uint64_t routeId) const { return routeId == m_routeId; }

  /// \returns Length of the route segment with |segIdx| in meters.
  double GetSegLenMeters(size_t segIdx) const;

  void SetMwmsPartlyProhibitedForSpeedCams(std::vector<platform::CountryFile> && mwms);

  /// \returns true if the route crosses at least one mwm where there are restrictions on warning
  /// about speed cameras.
  bool CrossMwmsPartlyProhibitedForSpeedCams() const;

  /// \returns mwm list which is crossed by the route and where there are restrictions on warning
  /// about speed cameras.
  std::vector<platform::CountryFile> const & GetMwmsPartlyProhibitedForSpeedCams() const;

  std::string DebugPrintTurns() const;

private:
  friend std::string DebugPrint(Route const & r);

  double GetPolySegAngle(size_t ind) const;
  void GetClosestTurnAfterIdx(size_t segIdx, turns::TurnItem & turn) const;

  /// \returns Estimated time from the beginning.
  double GetCurrentTimeFromBeginSec() const;

  std::string m_router;
  RoutingSettings m_routingSettings;
  std::string m_name;

  FollowedPolyline m_poly;

  std::set<std::string> m_absentCountries;
  std::vector<RouteSegment> m_routeSegments;
  // |m_haveAltitudes| is true if and only if all route points have altitude information.
  bool m_haveAltitudes = false;

  // Subroute
  SubrouteUid m_subrouteUid = kInvalidSubrouteId;
  size_t m_currentSubrouteIdx = 0;
  std::vector<SubrouteAttrs> m_subrouteAttrs;
  // Route identifier. It's unique within single program session.
  uint64_t m_routeId = 0;

  // Mwms which are crossed by the route where speed cameras are prohibited.
  std::vector<platform::CountryFile> m_speedCamPartlyProhibitedMwms;
};

/// \returns true if |turn| is not equal to turns::CarDirection::None or
/// |turns::PedestrianDirection::None|.
bool IsNormalTurn(turns::TurnItem const & turn);
} // namespace routing
