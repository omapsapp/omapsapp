#include "routing/routing_session.hpp"

#include "routing/routing_helpers.hpp"
#include "routing/speed_camera.hpp"

#include "platform/location.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"
#include "platform/distance.hpp"

#include "geometry/angles.hpp"
#include "geometry/mercator.hpp"

#include "indexer/road_shields_parser.hpp"

#include <utility>

namespace
{
int constexpr kOnRouteMissedCount = 10;

double constexpr kShowLanesMinDistInMeters = 500.0;

// @TODO The distance may depend on the current speed.
double constexpr kShowPedestrianTurnInMeters = 20.0;

double constexpr kRunawayDistanceSensitivityMeters = 0.01;

double constexpr kCompletionPercentAccuracy = 5;

double constexpr kMinimumETASec = 60.0;
}  // namespace

namespace routing
{
using namespace location;
using namespace traffic;

void FormatDistance(double dist, std::string & value, std::string & suffix)
{
  platform::Distance d = platform::Distance::CreateFormatted(dist);
  value = d.GetDistanceString();
  suffix = d.GetUnitsString();
}

RoutingSession::RoutingSession()
  : m_router(nullptr)
  , m_route(std::make_shared<Route>(std::string{} /* router */, 0 /* route id */))
  , m_state(SessionState::NoValidRoute)
  , m_isFollowing(false)
  , m_speedCameraManager(m_turnNotificationsMgr)
  , m_routingSettings(GetRoutingSettings(VehicleType::Car))
  , m_passedDistanceOnRouteMeters(0.0)
  , m_lastCompletionPercent(0.0)
{
  // To call |m_changeSessionStateCallback| on |m_state| initialization.
  SetState(SessionState::NoValidRoute);
  m_speedCameraManager.SetRoute(m_route);
}

void RoutingSession::Init(PointCheckCallback const & pointCheckCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(!m_router, ());
  m_router = std::make_unique<AsyncRouter>(pointCheckCallback);
}

void RoutingSession::BuildRoute(Checkpoints const & checkpoints, uint32_t timeoutSec)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(m_router, ());
  m_checkpoints = checkpoints;
  m_router->ClearState();

  m_isFollowing = false;
  m_routingRebuildCount = -1; // -1 for the first rebuild.

  RebuildRoute(checkpoints.GetStart(), m_buildReadyCallback, m_needMoreMapsCallback,
               m_removeRouteCallback, timeoutSec, SessionState::RouteBuilding, false /* adjust */);
}

void RoutingSession::RebuildRoute(m2::PointD const & startPoint,
                                  ReadyCallback const & readyCallback,
                                  NeedMoreMapsCallback const & needMoreMapsCallback,
                                  RemoveRouteCallback const & removeRouteCallback,
                                  uint32_t timeoutSec, SessionState routeRebuildingState,
                                  bool adjustToPrevRoute)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(m_router, ());
  SetState(routeRebuildingState);

  ++m_routingRebuildCount;
  auto const & direction = m_routingSettings.m_useDirectionForRouteBuilding
                               ? m_positionAccumulator.GetDirection()
                               : m2::PointD::Zero();

  Checkpoints checkpoints(m_checkpoints);
  checkpoints.SetPointFrom(startPoint);
  // Use old-style callback construction, because lambda constructs buggy function on Android
  // (callback param isn't captured by value).
  m_router->CalculateRoute(checkpoints, direction, adjustToPrevRoute,
                           DoReadyCallback(*this, readyCallback), needMoreMapsCallback,
                           removeRouteCallback, m_progressCallback, timeoutSec);
}

m2::PointD RoutingSession::GetStartPoint() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_checkpoints.GetStart();
}

m2::PointD RoutingSession::GetEndPoint() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_checkpoints.GetFinish();
}

void RoutingSession::DoReadyCallback::operator()(std::shared_ptr<Route> const & route, RouterResultCode e)
{
  ASSERT(m_rs.m_route, ());
  m_rs.AssignRoute(route, e);
  m_callback(*m_rs.m_route, e);
}

void RoutingSession::RemoveRoute()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_lastDistance = 0.0;
  m_moveAwayCounter = 0;
  m_turnNotificationsMgr.Reset();

  m_route = std::make_shared<Route>(std::string{} /* router */, 0 /* route id */);
  m_speedCameraManager.Reset();
  m_speedCameraManager.SetRoute(m_route);
}

void RoutingSession::RebuildRouteOnTrafficUpdate()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m2::PointD startPoint;

  {
    startPoint = m_lastGoodPosition;

    switch (m_state)
    {
    case SessionState::NoValidRoute:
    case SessionState::RouteFinished: return;

    case SessionState::RouteBuilding:
    case SessionState::RouteNotStarted:
    case SessionState::RouteNoFollowing:
    case SessionState::RouteRebuilding:
      startPoint = m_checkpoints.GetPointFrom();
      break;

    case SessionState::OnRoute:
    case SessionState::RouteNeedRebuild: break;
    }

    // Cancel current route building.
    m_router->ClearState();
  }

  RebuildRoute(startPoint, m_rebuildReadyCallback, nullptr /* needMoreMapsCallback */,
               nullptr /* removeRouteCallback */, RouterDelegate::kNoTimeout,
               SessionState::RouteRebuilding, false /* adjustToPrevRoute */);
}

bool RoutingSession::IsActive() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state != SessionState::NoValidRoute);
}

bool RoutingSession::IsNavigable() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state == SessionState::RouteNotStarted || m_state == SessionState::OnRoute ||
          m_state == SessionState::RouteFinished);
}

bool RoutingSession::IsBuilt() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (IsNavigable() || m_state == SessionState::RouteNeedRebuild);
}

bool RoutingSession::IsBuilding() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state == SessionState::RouteBuilding || m_state == SessionState::RouteRebuilding);
}

bool RoutingSession::IsBuildingOnly() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteBuilding;
}

bool RoutingSession::IsRebuildingOnly() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteRebuilding;
}

bool RoutingSession::IsFinished() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteFinished;
}

bool RoutingSession::IsNoFollowing() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteNoFollowing;
}

bool RoutingSession::IsOnRoute() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state == SessionState::OnRoute);
}

bool RoutingSession::IsFollowing() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_isFollowing;
}

void RoutingSession::Reset()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_router != nullptr, ());

  RemoveRoute();
  SetState(SessionState::NoValidRoute);
  m_router->ClearState();

  m_passedDistanceOnRouteMeters = 0.0;
  m_isFollowing = false;
  m_lastCompletionPercent = 0;
}

void RoutingSession::SetState(SessionState state)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  if (m_changeSessionStateCallback && m_state != state)
    m_changeSessionStateCallback(m_state, state);

  m_state = state;
}

SessionState RoutingSession::OnLocationPositionChanged(GpsInfo const & info)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT_NOT_EQUAL(m_state, SessionState::NoValidRoute, ());
  ASSERT(m_router, ());

  if (m_state == SessionState::RouteFinished || m_state == SessionState::RouteBuilding ||
      m_state == SessionState::RouteNoFollowing || m_state == SessionState::NoValidRoute)
  {
    return m_state;
  }

  CHECK(m_route, (m_state));
  // Note. The route may not be valid here. It happens in case when while the first route
  // build is cancelled because of traffic jam were downloaded. After that route rebuilding
  // happens. While the rebuilding may be called OnLocationPositionChanged(...)
  if (!m_route->IsValid())
    return m_state;

  m_turnNotificationsMgr.SetSpeedMetersPerSecond(info.m_speedMpS);

  auto const formerIter = m_route->GetCurrentIteratorTurn();
  if (m_route->MoveIterator(info))
  {
    m_moveAwayCounter = 0;
    m_lastDistance = 0.0;

    PassCheckpoints();

    if (m_checkpoints.IsFinished())
    {
      m_passedDistanceOnRouteMeters += m_route->GetTotalDistanceMeters();
      SetState(SessionState::RouteFinished);
    }
    else
    {
      SetState(SessionState::OnRoute);
      m_speedCameraManager.OnLocationPositionChanged(info);
    }

    if (m_userCurrentPositionValid)
      m_lastGoodPosition = m_userCurrentPosition;

    auto const curIter = m_route->GetCurrentIteratorTurn();
    // If we are moving to the next segment after passing the turn
    // it means the turn is changed. So the |m_onNewTurn| should be called.
    if (formerIter && curIter && IsNormalTurn(*formerIter) &&
        formerIter->m_index < curIter->m_index && m_onNewTurn)
    {
      m_onNewTurn();
    }

    return m_state;
  }

  if (m_state != SessionState::RouteNeedRebuild && m_state != SessionState::RouteRebuilding)
  {
    // Distance from the last known projection on route
    // (check if we are moving far from the last known projection).
    auto const & lastGoodPoint = m_route->GetFollowedPolyline().GetCurrentIter().m_pt;
    double const dist = mercator::DistanceOnEarth(lastGoodPoint,
                                                  mercator::FromLatLon(info.m_latitude, info.m_longitude));
    if (base::AlmostEqualAbs(dist, m_lastDistance, kRunawayDistanceSensitivityMeters))
      return m_state;

    if (!info.HasSpeed() || info.m_speedMpS < m_routingSettings.m_minSpeedForRouteRebuildMpS)
      m_moveAwayCounter += 1;
    else
      m_moveAwayCounter += 2;

    m_lastDistance = dist;

    if (m_moveAwayCounter > kOnRouteMissedCount)
    {
      m_passedDistanceOnRouteMeters += m_route->GetCurrentDistanceFromBeginMeters();
      SetState(SessionState::RouteNeedRebuild);
    }
  }

  return m_state;
}

// For next street returns "[ref] name" .
// For highway exits (or main roads with exit info) returns "[junction:ref]: [target:ref] > target".
// If no |target| - it will be replaced by |name| of next street.
// If no |target:ref| - it will be replaced by |ref| of next road.
// So if link has no info at all, "[ref] name" of next will be returned (as for next street).
void GetFullRoadName(RouteSegment::RoadNameInfo & road, std::string & name)
{
  if (auto const & sh = ftypes::GetRoadShields(road.m_ref); !sh.empty())
    road.m_ref = sh[0].m_name;
  if (auto const & sh = ftypes::GetRoadShields(road.m_destination_ref); !sh.empty())
    road.m_destination_ref = sh[0].m_name;

  name.clear();
  if (road.HasExitInfo())
  {
    if (!road.m_junction_ref.empty())
      name = "[" + road.m_junction_ref + "]";

    if (!road.m_destination_ref.empty())
      name += std::string(name.empty() ? "" : ": ") + "[" + road.m_destination_ref + "]";

    if (!road.m_destination.empty())
      name += std::string(name.empty() ? "" : " ") + "> " + road.m_destination;
    else if (!road.m_name.empty())
      name += (road.m_destination_ref.empty() ? std::string(name.empty() ? "" : " ") : ": ") + road.m_name;
  }
  else
  {
    if (!road.m_ref.empty())
      name = "[" + road.m_ref + "]";
    if (!road.m_name.empty())
      name += (name.empty() ? "" : " ") + road.m_name;
  }
}

void RoutingSession::GetRouteFollowingInfo(FollowingInfo & info) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  ASSERT(m_route, ());

  if (!m_route->IsValid())
  {
    // nothing should be displayed on the screen about turns if these lines are executed
    info = FollowingInfo();
    return;
  }

  if (!IsNavigable())
  {
    info = FollowingInfo();
    info.m_distToTarget = platform::Distance::CreateFormatted(m_route->GetTotalDistanceMeters());
    info.m_time = static_cast<int>(std::max(kMinimumETASec, m_route->GetCurrentTimeToEndSec()));
    return;
  }

  info.m_distToTarget =
      platform::Distance::CreateFormatted(m_route->GetCurrentDistanceToEndMeters());

  double distanceToTurnMeters = 0.;
  turns::TurnItem turn;
  m_route->GetNearestTurn(distanceToTurnMeters, turn);
  info.m_distToTurn = platform::Distance::CreateFormatted(distanceToTurnMeters);
  info.m_turn = turn.m_turn;

  SpeedInUnits speedLimit;
  m_route->GetCurrentSpeedLimit(speedLimit);
  if (speedLimit.IsNumeric())
    info.m_speedLimitMps = measurement_utils::KmphToMps(speedLimit.GetSpeedKmPH());
  else if (speedLimit.GetSpeed() == kNoneMaxSpeed)
    info.m_speedLimitMps = 0;
  else
    info.m_speedLimitMps = -1.0;

  // The turn after the next one.
  if (m_routingSettings.m_showTurnAfterNext)
    info.m_nextTurn = m_turnNotificationsMgr.GetSecondTurnNotification();
  else
    info.m_nextTurn = routing::turns::CarDirection::None;

  info.m_exitNum = turn.m_exitNum;
  info.m_time = static_cast<int>(std::max(kMinimumETASec, m_route->GetCurrentTimeToEndSec()));
  RouteSegment::RoadNameInfo sourceRoadNameInfo, targetRoadNameInfo;
  m_route->GetCurrentStreetName(sourceRoadNameInfo);
  GetFullRoadName(sourceRoadNameInfo, info.m_sourceName);
  m_route->GetNextTurnStreetName(targetRoadNameInfo);
  GetFullRoadName(targetRoadNameInfo, info.m_targetName);

  info.m_completionPercent = GetCompletionPercent();

  double const timeToNearestTurnSec = m_route->GetCurrentTimeToNearestTurnSec();

  // Lane information and next street name.
  if (distanceToTurnMeters < kShowLanesMinDistInMeters || timeToNearestTurnSec < 60.0)
  {
    info.m_displayedStreetName = info.m_targetName;
    info.m_lanes = turn.m_lanes;
  }
  else
  {
    info.m_displayedStreetName = "";
    info.m_lanes.clear();
  }

  // Pedestrian info.
  info.m_pedestrianTurn = (distanceToTurnMeters < kShowPedestrianTurnInMeters)
                              ? turn.m_pedestrianTurn
                              : turns::PedestrianDirection::None;
}

double RoutingSession::GetCompletionPercent() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());

  double const denominator = m_passedDistanceOnRouteMeters + m_route->GetTotalDistanceMeters();
  if (!m_route->IsValid() || denominator == 0.0)
    return 0;

  double const percent =
      100.0 * (m_passedDistanceOnRouteMeters + m_route->GetCurrentDistanceFromBeginMeters()) /
      denominator;
  if (percent - m_lastCompletionPercent > kCompletionPercentAccuracy)
  {
    m_lastCompletionPercent = percent;
  }
  return percent;
}

void RoutingSession::PassCheckpoints()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  while (!m_checkpoints.IsFinished() && m_route->IsSubroutePassed(m_checkpoints.GetPassedIdx()))
  {
    m_route->PassNextSubroute();
    m_checkpoints.PassNextPoint();
    LOG(LINFO, ("Pass checkpoint, ", m_checkpoints));
    m_checkpointCallback(m_checkpoints.GetPassedIdx());
  }
}

void RoutingSession::GenerateNotifications(std::vector<std::string> & notifications, bool announceStreets)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  notifications.clear();

  ASSERT(m_route, ());

  // Voice turn notifications.
  if (!m_routingSettings.m_soundDirection)
    return;

  if (!m_route->IsValid() || !IsNavigable())
    return;

  // Generate turns notifications.
  std::vector<turns::TurnItemDist> turns;
  if (m_route->GetNextTurns(turns))
  {
    RouteSegment::RoadNameInfo nextStreetInfo;

    // only populate nextStreetInfo if TtsStreetNames is enabled
    if (announceStreets)
      m_route->GetNextTurnStreetName(nextStreetInfo);

    m_turnNotificationsMgr.GenerateTurnNotifications(turns, notifications, nextStreetInfo);
  }

  m_speedCameraManager.GenerateNotifications(notifications);
}

void RoutingSession::AssignRoute(std::shared_ptr<Route> const & route, RouterResultCode e)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (e != RouterResultCode::NoError)
  {
    // Route building was not success. If the former route is valid let's continue moving along it.
    // If not, let's set corresponding state.
    if (m_route->IsValid())
      SetState(SessionState::OnRoute);
    else
      SetState(SessionState::NoValidRoute);
    return;
  }

  RemoveRoute();
  SetState(SessionState::RouteNotStarted);
  m_lastCompletionPercent = 0;
  m_checkpoints.SetPointFrom(route->GetPoly().Front());

  route->SetRoutingSettings(m_routingSettings);
  m_route = route;
  m_speedCameraManager.Reset();
  m_speedCameraManager.SetRoute(m_route);
}

void RoutingSession::SetRouter(std::unique_ptr<IRouter> && router,
                               std::unique_ptr<AbsentRegionsFinder> && finder)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_router != nullptr, ());
  Reset();
  m_router->SetRouter(std::move(router), std::move(finder));
}

void RoutingSession::MatchLocationToRoadGraph(location::GpsInfo & location)
{
  auto const locationMerc = mercator::FromLatLon(location.m_latitude, location.m_longitude);
  double const radius = m_route->GetCurrentRoutingSettings().m_matchingThresholdM;

  m2::PointD const direction = m_positionAccumulator.GetDirection();

  static auto constexpr kEps = 1e-5;
  if (base::AlmostEqualAbs(direction, m2::PointD::Zero(), kEps))
    return;

  EdgeProj proj;
  if (!m_router->FindClosestProjectionToRoad(locationMerc, direction, radius, proj))
  {
    m_projectedToRoadGraph = false;
    return;
  }

  // First matching to the same road. We pull coordinates and angle on the following matchings.
  if (!m_projectedToRoadGraph)
  {
    m_projectedToRoadGraph = true;
    m_proj = proj;
    return;
  }

  if (m_proj.m_edge.GetFeatureId() == proj.m_edge.GetFeatureId())
  {
    if (m_route->GetCurrentRoutingSettings().m_matchRoute)
    {
      if (!base::AlmostEqualAbs(m_proj.m_point, proj.m_point, kEps))
      {
        location.m_bearing =
            location::AngleToBearing(base::RadToDeg(ang::AngleTo(m_proj.m_point, proj.m_point)));
      }
    }

    location.m_latitude = mercator::YToLat(proj.m_point.y);
    location.m_longitude = mercator::XToLon(proj.m_point.x);
  }
  else
  {
    m_projectedToRoadGraph = false;
  }

  m_proj = proj;
}

bool RoutingSession::MatchLocationToRoute(location::GpsInfo & location,
                                          location::RouteMatchingInfo & routeMatchingInfo)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  if (!IsOnRoute())
    return true;

  ASSERT(m_route, ());

  bool const matchedToRoute = m_route->MatchLocationToRoute(location, routeMatchingInfo);
  if (matchedToRoute)
    m_projectedToRoadGraph = false;

  return matchedToRoute;
}

traffic::SpeedGroup RoutingSession::MatchTraffic(
    location::RouteMatchingInfo const & routeMatchingInfo) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  if (!routeMatchingInfo.IsMatched())
    return SpeedGroup::Unknown;

  size_t const index = routeMatchingInfo.GetIndexInRoute();

  return m_route->GetTraffic(index);
}

bool RoutingSession::DisableFollowMode()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  LOG(LINFO, ("Routing disables a following mode. SessionState: ", m_state));
  if (m_state == SessionState::RouteNotStarted || m_state == SessionState::OnRoute)
  {
    SetState(SessionState::RouteNoFollowing);
    m_isFollowing = false;
    return true;
  }
  return false;
}

bool RoutingSession::EnableFollowMode()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  LOG(LINFO, ("Routing enables a following mode. SessionState: ", m_state));
  if (m_state == SessionState::RouteNotStarted || m_state == SessionState::OnRoute)
  {
    SetState(SessionState::OnRoute);
    m_isFollowing = true;
  }
  return m_isFollowing;
}

void RoutingSession::SetRoutingSettings(RoutingSettings const & routingSettings)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_routingSettings = routingSettings;
}

void RoutingSession::SetRoutingCallbacks(ReadyCallback const & buildReadyCallback,
                                         ReadyCallback const & rebuildReadyCallback,
                                         NeedMoreMapsCallback const & needMoreMapsCallback,
                                         RemoveRouteCallback const & removeRouteCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_buildReadyCallback = buildReadyCallback;
  m_rebuildReadyCallback = rebuildReadyCallback;
  m_needMoreMapsCallback = needMoreMapsCallback;
  m_removeRouteCallback = removeRouteCallback;
}

void RoutingSession::SetSpeedCamShowCallback(SpeedCameraShowCallback && callback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_speedCameraManager.SetSpeedCamShowCallback(std::move(callback));
}

void RoutingSession::SetSpeedCamClearCallback(SpeedCameraClearCallback && callback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_speedCameraManager.SetSpeedCamClearCallback(std::move(callback));
}

void RoutingSession::SetProgressCallback(ProgressCallback const & progressCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_progressCallback = progressCallback;
}

void RoutingSession::SetCheckpointCallback(CheckpointCallback const & checkpointCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_checkpointCallback = checkpointCallback;
}

void RoutingSession::SetChangeSessionStateCallback(
    ChangeSessionStateCallback const & changeSessionStateCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_changeSessionStateCallback = changeSessionStateCallback;
}

void RoutingSession::SetOnNewTurnCallback(OnNewTurn const & onNewTurn)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_onNewTurn = onNewTurn;
}

void RoutingSession::SetUserCurrentPosition(m2::PointD const & position)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_userCurrentPosition = position;
  m_userCurrentPositionValid = true;
}

void RoutingSession::PushPositionAccumulator(m2::PointD const & position)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_positionAccumulator.PushNextPoint(position);
}
void RoutingSession::ClearPositionAccumulator()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_positionAccumulator.Clear();
}

void RoutingSession::EnableTurnNotifications(bool enable)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_turnNotificationsMgr.Enable(enable);
}

bool RoutingSession::AreTurnNotificationsEnabled() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_turnNotificationsMgr.IsEnabled();
}

void RoutingSession::SetTurnNotificationsUnits(measurement_utils::Units const units)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_turnNotificationsMgr.SetLengthUnits(units);
}

void RoutingSession::SetTurnNotificationsLocale(std::string const & locale)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  LOG(LINFO, ("The language for turn notifications is", locale));
  m_turnNotificationsMgr.SetLocale(locale);
}

std::string RoutingSession::GetTurnNotificationsLocale() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_turnNotificationsMgr.GetLocale();
}

void RoutingSession::RouteCall(RouteCallback const & callback) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(m_route, ());
  callback(*m_route);
}

void RoutingSession::EmitCloseRoutingEvent() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());

  if (!m_route->IsValid())
  {
    ASSERT(false, ());
    return;
  }
}

bool RoutingSession::HasRouteAltitude() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());
  return m_route->HaveAltitudes();
}

bool RoutingSession::IsRouteId(uint64_t routeId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_route->IsRouteId(routeId);
}

bool RoutingSession::IsRouteValid() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_route && m_route->IsValid();
}

bool RoutingSession::GetRouteJunctionPoints(std::vector<m2::PointD> & routeJunctionPoints) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());

  if (!m_route->IsValid())
    return false;

  auto const & segments = m_route->GetRouteSegments();
  routeJunctionPoints.reserve(segments.size());

  for (size_t i = 0; i < segments.size(); ++i)
  {
    auto const & junction = segments[i].GetJunction();
    routeJunctionPoints.push_back(junction.GetPoint());
  }

  ASSERT_EQUAL(routeJunctionPoints.size(), routeJunctionPoints.size(), ());
  return true;
}

bool RoutingSession::GetRouteAltitudesAndDistancesM(std::vector<double> & routeSegDistanceM,
                                                    geometry::Altitudes & routeAltitudesM) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());

  if (!m_route->IsValid() || !m_route->HaveAltitudes())
    return false;

  auto const & distances = m_route->GetSegDistanceMeters();
  routeSegDistanceM.reserve(distances.size() + 1);
  routeSegDistanceM.push_back(0);
  routeSegDistanceM.insert(routeSegDistanceM.end(), distances.begin(), distances.end());

  m_route->GetAltitudes(routeAltitudesM);

  ASSERT_EQUAL(routeSegDistanceM.size(), routeAltitudesM.size(), ());
  return true;
}

void RoutingSession::OnTrafficInfoClear()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  Clear();
  RebuildRouteOnTrafficUpdate();
}

void RoutingSession::OnTrafficInfoAdded(TrafficInfo && info)
{
  TrafficInfo::Coloring const & fullColoring = info.GetColoring();
  auto coloring = std::make_shared<TrafficInfo::Coloring>();
  for (auto const & kv : fullColoring)
  {
    ASSERT_NOT_EQUAL(kv.second, SpeedGroup::Unknown, ());
    coloring->insert(kv);
  }

  // Note. |coloring| should not be used after this call on gui thread.
  auto const mwmId = info.GetMwmId();
  GetPlatform().RunTask(Platform::Thread::Gui, [this, mwmId, coloring]() {
    Set(mwmId, coloring);
    RebuildRouteOnTrafficUpdate();
  });
}

void RoutingSession::OnTrafficInfoRemoved(MwmSet::MwmId const & mwmId)
{
  GetPlatform().RunTask(Platform::Thread::Gui, [this, mwmId]() {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    Remove(mwmId);
    RebuildRouteOnTrafficUpdate();
  });
}

void RoutingSession::CopyTraffic(traffic::AllMwmTrafficInfo & trafficColoring) const
{
  TrafficCache::CopyTraffic(trafficColoring);
}

void RoutingSession::SetLocaleWithJsonForTesting(std::string const & json, std::string const & locale)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_turnNotificationsMgr.SetLocaleWithJsonForTesting(json, locale);
}

std::string DebugPrint(SessionState state)
{
  switch (state)
  {
  case SessionState::NoValidRoute: return "NoValidRoute";
  case SessionState::RouteBuilding: return "RouteBuilding";
  case SessionState::RouteNotStarted: return "RouteNotStarted";
  case SessionState::OnRoute: return "OnRoute";
  case SessionState::RouteNeedRebuild: return "RouteNeedRebuild";
  case SessionState::RouteFinished: return "RouteFinished";
  case SessionState::RouteNoFollowing: return "RouteNoFollowing";
  case SessionState::RouteRebuilding: return "RouteRebuilding";
  }
  UNREACHABLE();
}
}  // namespace routing
