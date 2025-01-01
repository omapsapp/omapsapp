#pragma once

#include "routing/async_router.hpp"
#include "routing/following_info.hpp"
#include "routing/position_accumulator.hpp"
#include "routing/route.hpp"
#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_exceptions.hpp"
#include "routing/speed_camera.hpp"
#include "routing/speed_camera_manager.hpp"
#include "routing/turns.hpp"
#include "routing/turns_notification_manager.hpp"

#include "traffic/speed_groups.hpp"
#include "traffic/traffic_cache.hpp"
#include "traffic/traffic_info.hpp"

#include "platform/location.hpp"
#include "platform/measurement_utils.hpp"

#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"
#include "geometry/polyline2d.hpp"

#include "base/thread_checker.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <string>

namespace location
{
class RouteMatchingInfo;
}

namespace routing
{
/// \breaf This class is responsible for the route built in the program.
/// \note All method of this class should be called from ui thread if there's no
/// a special note near a method.
class RoutingSession : public traffic::TrafficObserver, public traffic::TrafficCache
{
  friend struct UnitClass_AsyncGuiThreadTestWithRoutingSession_TestFollowRoutePercentTest;

public:
  RoutingSession();

  void Init(PointCheckCallback const & pointCheckCallback);

  void SetRouter(std::unique_ptr<IRouter> && router,
                 std::unique_ptr<AbsentRegionsFinder> && finder);

  /// @param[in] checkpoints in mercator
  /// @param[in] timeoutSec timeout in seconds, if zero then there is no timeout
  void BuildRoute(Checkpoints const & checkpoints, uint32_t timeoutSec);
  void RebuildRoute(m2::PointD const & startPoint, ReadyCallback const & readyCallback,
                    NeedMoreMapsCallback const & needMoreMapsCallback,
                    RemoveRouteCallback const & removeRouteCallback, uint32_t timeoutSec,
                    SessionState routeRebuildingState, bool adjustToPrevRoute);

  m2::PointD GetStartPoint() const;
  m2::PointD GetEndPoint() const;

  bool IsActive() const;
  bool IsNavigable() const;
  bool IsBuilt() const;
  /// \returns true if a new route is in process of building rebuilding or
  /// if a route is being rebuilt in case the user left the route, and false otherwise.
  bool IsBuilding() const;
  bool IsBuildingOnly() const;
  bool IsRebuildingOnly() const;
  bool IsFinished() const;
  bool IsNoFollowing() const;
  bool IsOnRoute() const;
  bool IsFollowing() const;
  void Reset();

  void SetState(SessionState state);

  /// \returns true if altitude information along |m_route| is available and
  /// false otherwise.
  bool HasRouteAltitude() const;
  bool IsRouteId(uint64_t routeId) const;
  bool IsRouteValid() const;

  /// \brief copies distance from route beginning to ends of route segments in meters and
  /// route altitude information to |routeSegDistanceM| and |routeAltitudes|.
  /// \returns true if there is valid route information. If the route is not valid returns false.
  bool GetRouteAltitudesAndDistancesM(std::vector<double> & routeSegDistanceM,
                                      geometry::Altitudes & routeAltitudesM) const;

  /// \brief returns coordinates of route junctions.
  /// \returns true if there is valid route information. If the route is not valid returns false.
  bool GetRouteJunctionPoints(std::vector<m2::PointD> & routeJunctionPoints) const;

  SessionState OnLocationPositionChanged(location::GpsInfo const & info);
  void GetRouteFollowingInfo(FollowingInfo & info) const;

  bool MatchLocationToRoute(location::GpsInfo & location,
                            location::RouteMatchingInfo & routeMatchingInfo);
  void MatchLocationToRoadGraph(location::GpsInfo & location);
  // Get traffic speed for the current route position.
  // Returns SpeedGroup::Unknown if any trouble happens: position doesn't match with route or something else.
  traffic::SpeedGroup MatchTraffic(location::RouteMatchingInfo const & routeMatchingInfo) const;

  void SetUserCurrentPosition(m2::PointD const & position);

  void PushPositionAccumulator(m2::PointD const & position);
  void ClearPositionAccumulator();

  void ActivateAdditionalFeatures() {}

  /// Disable following mode on GPS updates. Following mode is disabled only for the current route.
  /// If a route is rebuilt you must call DisableFollowMode again.
  /// Returns true if following was disabled, false if a route is not ready for the following yet.
  bool DisableFollowMode();

  /// Now indicates only that user pushed start button. Route follows by default.
  /// Returns true if following was enabled, false if a route is not ready for the following yet.
  bool EnableFollowMode();

  void SetRoutingSettings(RoutingSettings const & routingSettings);
  void SetRoutingCallbacks(ReadyCallback const & buildReadyCallback,
                           ReadyCallback const & rebuildReadyCallback,
                           NeedMoreMapsCallback const & needMoreMapsCallback,
                           RemoveRouteCallback const & removeRouteCallback);
  void SetProgressCallback(ProgressCallback const & progressCallback);
  void SetCheckpointCallback(CheckpointCallback const & checkpointCallback);
  /// \brief Sets a callback which is called every time when RoutingSession::m_state is changed.
  void SetChangeSessionStateCallback(ChangeSessionStateCallback const & changeSessionStateCallback);
  void SetOnNewTurnCallback(OnNewTurn const & onNewTurn);

  void SetSpeedCamShowCallback(SpeedCameraShowCallback && callback);
  void SetSpeedCamClearCallback(SpeedCameraClearCallback && callback);

  // Sound notifications for turn instructions.
  void GenerateNotifications(std::vector<std::string> & notifications, bool announceStreets);
  void EnableTurnNotifications(bool enable);
  void SetTurnNotificationsUnits(measurement_utils::Units const units);
  void SetTurnNotificationsLocale(std::string const & locale);
  bool AreTurnNotificationsEnabled() const;
  std::string GetTurnNotificationsLocale() const;
  void SetLocaleWithJsonForTesting(std::string const & json, std::string const & locale);

  void EmitCloseRoutingEvent() const;

  void RouteCall(RouteCallback const & callback) const;

  // RoutingObserver overrides:
  void OnTrafficInfoClear() override;
  /// \note. This method may be called from any thread because it touches class data on gui thread.
  void OnTrafficInfoAdded(traffic::TrafficInfo && info) override;
  /// \note. This method may be called from any thread because it touches class data on gui thread.
  void OnTrafficInfoRemoved(MwmSet::MwmId const & mwmId) override;

  // TrafficCache overrides:
  /// \note. This method may be called from any thread because it touches only data
  /// protected by mutex in TrafficCache class.
  void CopyTraffic(traffic::AllMwmTrafficInfo & trafficColoring) const override;

  void AssignRouteForTesting(std::shared_ptr<Route> route, RouterResultCode e) { AssignRoute(route, e); }

  bool IsSpeedCamLimitExceeded() const { return m_speedCameraManager.IsSpeedLimitExceeded(); }
  SpeedCameraManager & GetSpeedCamManager() { return m_speedCameraManager; }
  SpeedCameraManager const & GetSpeedCamManager() const { return m_speedCameraManager; }

  std::shared_ptr<Route> GetRouteForTests() const { return m_route; }
  void SetGuidesForTests(GuidesTracks guides) { m_router->SetGuidesTracks(std::move(guides)); }

  double GetCompletionPercent() const;

private:
  struct DoReadyCallback
  {
    RoutingSession & m_rs;
    ReadyCallback m_callback;

    DoReadyCallback(RoutingSession & rs, ReadyCallback const & cb)
        : m_rs(rs), m_callback(cb)
    {}

    void operator()(std::shared_ptr<Route> const & route, RouterResultCode e);
  };

  void AssignRoute(std::shared_ptr<Route> const & route, RouterResultCode e);
  /// RemoveRoute() removes m_route and resets route attributes (m_lastDistance, m_moveAwayCounter).
  void RemoveRoute();
  void RebuildRouteOnTrafficUpdate();

  void PassCheckpoints();

private:
  std::unique_ptr<AsyncRouter> m_router;
  std::shared_ptr<Route> m_route;
  SessionState m_state;
  bool m_isFollowing;
  Checkpoints m_checkpoints;

  EdgeProj m_proj;
  bool m_projectedToRoadGraph = false;

  /// Current position metrics to check for RouteNeedRebuild state.
  double m_lastDistance = 0.0;
  int m_moveAwayCounter = 0;
  m2::PointD m_lastGoodPosition;

  m2::PointD m_userCurrentPosition;
  bool m_userCurrentPositionValid = false;

  // Sound turn notification parameters.
  turns::sound::NotificationManager m_turnNotificationsMgr;

  SpeedCameraManager m_speedCameraManager;
  RoutingSettings m_routingSettings;

  PositionAccumulator m_positionAccumulator;

  ReadyCallback m_buildReadyCallback;
  ReadyCallback m_rebuildReadyCallback;
  NeedMoreMapsCallback m_needMoreMapsCallback;
  RemoveRouteCallback m_removeRouteCallback;
  ProgressCallback m_progressCallback;
  CheckpointCallback m_checkpointCallback;
  ChangeSessionStateCallback m_changeSessionStateCallback;
  OnNewTurn m_onNewTurn;

  // Statistics parameters
  // Passed distance on route including reroutes
  double m_passedDistanceOnRouteMeters = 0.0;
  // Rerouting count
  int m_routingRebuildCount = -1; // -1 for the first rebuild called in BuildRoute().
  int m_routingRebuildAnnounceCount = 0; // track TTS announcement state (ignore the first build)
  mutable double m_lastCompletionPercent = 0.0;

  DECLARE_THREAD_CHECKER(m_threadChecker);
};

void FormatDistance(double dist, std::string & value, std::string & suffix);

std::string DebugPrint(SessionState state);
}  // namespace routing
