#include "routing_manager.hpp"

#include "map/chart_generator.hpp"
#include "map/routing_mark.hpp"

#include "routing/absent_regions_finder.hpp"
#include "routing/checkpoint_predictor.hpp"
#include "routing/index_router.hpp"
#include "routing/route.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/ruler_router.hpp"
#include "routing/speed_camera.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/routing_helpers.hpp"
#include "storage/storage.hpp"

#include "drape_frontend/drape_engine.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "indexer/map_style_reader.hpp"

#include "platform/country_file.hpp"
#include "platform/platform.hpp"
#include "platform/socket.hpp"

#include "geometry/mercator.hpp"  // kPointEqualityEps
#include "geometry/simplification.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"

#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

#include <iomanip>
#include <ios>
#include <map>
#include <sstream>

#include "cppjansson/cppjansson.hpp"

using namespace routing;
using namespace std;

namespace
{
char const kRouterTypeKey[] = "router";

double const kRouteScaleMultiplier = 1.5;

string const kRoutePointsFile = "route_points.dat";

uint32_t constexpr kInvalidTransactionId = 0;

void FillTurnsDistancesForRendering(vector<RouteSegment> const & segments,
                                    double baseDistance, vector<double> & turns)
{
  using namespace routing::turns;
  turns.clear();
  turns.reserve(segments.size());
  for (auto const & s : segments)
  {
    auto const & t = s.GetTurn();
    CHECK_NOT_EQUAL(t.m_turn, CarDirection::Count, ());
    // We do not render some of turn directions.
    if (t.m_turn == CarDirection::None || t.m_turn == CarDirection::StartAtEndOfStreet ||
        t.m_turn == CarDirection::StayOnRoundAbout || t.m_turn == CarDirection::ReachedYourDestination)
    {
      continue;
    }
    turns.push_back(s.GetDistFromBeginningMerc() - baseDistance);
  }
}

void FillTrafficForRendering(vector<RouteSegment> const & segments,
                             vector<traffic::SpeedGroup> & traffic)
{
  traffic.clear();
  traffic.reserve(segments.size());
  for (auto const & s : segments)
    traffic.push_back(s.GetTraffic());
}

RouteMarkData GetLastPassedPoint(BookmarkManager * bmManager, vector<RouteMarkData> const & points)
{
  ASSERT_GREATER_OR_EQUAL(points.size(), 2, ());
  ASSERT(points[0].m_pointType == RouteMarkType::Start, ());
  RouteMarkData data = points[0];

  for (int i = static_cast<int>(points.size()) - 1; i >= 0; i--)
  {
    if (points[i].m_isPassed)
    {
      data = points[i];
      break;
    }
  }

  // Last passed point will be considered as start point.
  data.m_pointType = RouteMarkType::Start;
  data.m_intermediateIndex = 0;
  if (data.m_isMyPosition)
  {
    data.m_position = bmManager->MyPositionMark().GetPivot();
    data.m_isMyPosition = false;
  }

  return data;
}

void SerializeRoutePoint(json_t * node, RouteMarkData const & data)
{
  ASSERT(node != nullptr, ());
  ToJSONObject(*node, "type", static_cast<int>(data.m_pointType));
  ToJSONObject(*node, "title", data.m_title);
  ToJSONObject(*node, "subtitle", data.m_subTitle);
  ToJSONObject(*node, "x", data.m_position.x);
  ToJSONObject(*node, "y", data.m_position.y);
}

RouteMarkData DeserializeRoutePoint(json_t * node)
{
  ASSERT(node != nullptr, ());
  RouteMarkData data;

  int type = 0;
  FromJSONObject(node, "type", type);
  data.m_pointType = static_cast<RouteMarkType>(type);

  FromJSONObject(node, "title", data.m_title);
  FromJSONObject(node, "subtitle", data.m_subTitle);

  FromJSONObject(node, "x", data.m_position.x);
  FromJSONObject(node, "y", data.m_position.y);

  return data;
}

string SerializeRoutePoints(vector<RouteMarkData> const & points)
{
  ASSERT_GREATER_OR_EQUAL(points.size(), 2, ());
  auto pointsNode = base::NewJSONArray();
  for (auto const & p : points)
  {
    auto pointNode = base::NewJSONObject();
    SerializeRoutePoint(pointNode.get(), p);
    json_array_append_new(pointsNode.get(), pointNode.release());
  }
  unique_ptr<char, JSONFreeDeleter> buffer(
    json_dumps(pointsNode.get(), JSON_COMPACT | JSON_ENSURE_ASCII));
  return string(buffer.get());
}

vector<RouteMarkData> DeserializeRoutePoints(string const & data)
{
  try
  {
    base::Json root(data.c_str());

    if (root.get() == nullptr || !json_is_array(root.get()))
      return {};

    size_t const sz = json_array_size(root.get());
    if (sz == 0)
      return {};

    vector<RouteMarkData> result;
    result.reserve(sz);
    for (size_t i = 0; i < sz; ++i)
    {
      auto pointNode = json_array_get(root.get(), i);
      if (pointNode == nullptr)
        continue;

      auto point = DeserializeRoutePoint(pointNode);
      if (point.m_position.EqualDxDy(m2::PointD::Zero(), mercator::kPointEqualityEps))
        continue;

      result.push_back(std::move(point));
    }

    if (result.size() < 2)
      return {};

    return result;
  }
  catch (base::Json::Exception const &)
  {
    return {};
  }
}

VehicleType GetVehicleType(RouterType routerType)
{
  switch (routerType)
  {
  case RouterType::Pedestrian: return VehicleType::Pedestrian;
  case RouterType::Bicycle: return VehicleType::Bicycle;
  case RouterType::Vehicle: return VehicleType::Car;
  case RouterType::Transit: return VehicleType::Transit;
  case RouterType::Ruler: return VehicleType::Transit;
  case RouterType::Count: CHECK(false, ("Invalid type", routerType)); return VehicleType::Count;
  }
  UNREACHABLE();
}

RoadWarningMarkType GetRoadType(RoutingOptions::Road road)
{
  if (road == RoutingOptions::Road::Toll)
    return RoadWarningMarkType::Toll;
  if (road == RoutingOptions::Road::Ferry)
    return RoadWarningMarkType::Ferry;
  if (road == RoutingOptions::Road::Dirty)
    return RoadWarningMarkType::Dirty;

  CHECK(false, ("Invalid road type to avoid:", road));
  return RoadWarningMarkType::Count;
}

drape_ptr<df::Subroute> CreateDrapeSubroute(vector<RouteSegment> const & segments, m2::PointD const & startPt,
                                            double baseDistance, double baseDepth, routing::RouterType routerType)
{
  auto subroute = make_unique_dp<df::Subroute>();
  subroute->m_baseDistance = baseDistance;
  subroute->m_baseDepthIndex = baseDepth;

  auto constexpr kBias = 1.0;

  if (routerType == RouterType::Transit)
  {
    subroute->m_headFakeDistance = -kBias;
    subroute->m_tailFakeDistance = kBias;
    subroute->m_polyline.Add(startPt);
    return subroute;
  }

  vector<m2::PointD> points;
  points.reserve(segments.size() + 1);
  points.push_back(startPt);
  for (auto const & s : segments)
    points.push_back(s.GetJunction().GetPoint());

  if (points.size() < 2)
  {
    LOG(LWARNING, ("Invalid subroute. Points number =", points.size()));
    return nullptr;
  }

  if (routerType == RouterType::Ruler)
  {
    auto const subrouteLen = segments.back().GetDistFromBeginningMerc() - baseDistance;
    subroute->m_headFakeDistance = -kBias;
    subroute->m_tailFakeDistance = subrouteLen + kBias;
    subroute->m_polyline = m2::PolylineD(std::move(points));
    return subroute;
  }

  // We support visualization of fake edges only in the head and in the tail of subroute.
  auto constexpr kInvalidId = std::numeric_limits<size_t>::max();
  auto firstReal = kInvalidId;
  auto lastReal = kInvalidId;
  for (size_t i = 0; i < segments.size(); ++i)
  {
    if (!segments[i].GetSegment().IsRealSegment())
      continue;

    if (firstReal == kInvalidId)
      firstReal = i;
    lastReal = i;
  }

  if (firstReal == kInvalidId)
  {
    // All segments are fake.
    subroute->m_headFakeDistance = 0.0;
    subroute->m_tailFakeDistance = 0.0;
  }
  else
  {
    CHECK_NOT_EQUAL(firstReal, kInvalidId, ());
    CHECK_NOT_EQUAL(lastReal, kInvalidId, ());

    auto constexpr kEps = 1e-5;

    // To prevent visual artefacts, in the case when all head segments are real
    // m_headFakeDistance must be less than 0.0.
    auto const headLen = (firstReal > 0) ? segments[firstReal - 1].GetDistFromBeginningMerc() - baseDistance : 0.0;
    if (base::AlmostEqualAbs(headLen, 0.0, kEps))
      subroute->m_headFakeDistance = -kBias;
    else
      subroute->m_headFakeDistance = headLen;

    // To prevent visual artefacts, in the case when all tail segments are real
    // m_tailFakeDistance must be greater than the length of the subroute.
    auto const subrouteLen = segments.back().GetDistFromBeginningMerc() - baseDistance;
    auto const tailLen = segments[lastReal].GetDistFromBeginningMerc() - baseDistance;
    if (base::AlmostEqualAbs(tailLen, subrouteLen, kEps))
      subroute->m_tailFakeDistance = subrouteLen + kBias;
    else
      subroute->m_tailFakeDistance = tailLen;
  }

  subroute->m_polyline = m2::PolylineD(std::move(points));
  return subroute;
}
}  // namespace

RoutingManager::RoutingManager(Callbacks && callbacks, Delegate & delegate)
  : m_callbacks(std::move(callbacks))
  , m_delegate(delegate)
  , m_extrapolator(
        [this](location::GpsInfo const & gpsInfo) { this->OnExtrapolatedLocationUpdate(gpsInfo); })
{
  m_routingSession.Init(
#ifdef SHOW_ROUTE_DEBUG_MARKS
                        [this](m2::PointD const & pt) {
                          if (m_bmManager == nullptr)
                            return;
                          auto editSession = m_bmManager->GetEditSession();
                          editSession.SetIsVisible(UserMark::Type::DEBUG_MARK, true);
                          editSession.CreateUserMark<DebugMarkPoint>(pt);
                        }
#else
                        nullptr
#endif
  );

  m_routingSession.SetRoutingCallbacks(
      [this](Route const & route, RouterResultCode code) { OnBuildRouteReady(route, code); },
      [this](Route const & route, RouterResultCode code) { OnRebuildRouteReady(route, code); },
      [this](uint64_t routeId, storage::CountriesSet const & absentCountries) {
        OnNeedMoreMaps(routeId, absentCountries);
      },
      [this](RouterResultCode code) { OnRemoveRoute(code); });

  m_routingSession.SetCheckpointCallback([this](size_t passedCheckpointIdx)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, passedCheckpointIdx]()
    {
      size_t const pointsCount = GetRoutePointsCount();

      // TODO(@bykoianko). Since routing system may invoke callbacks from different threads and here
      // we have to use gui thread, ASSERT is not correct. Uncomment it and delete condition after
      // refactoring of threads usage in routing system.
      //ASSERT_LESS(passedCheckpointIdx, pointsCount, ());
      if (passedCheckpointIdx >= pointsCount)
        return;

      if (passedCheckpointIdx == 0)
        OnRoutePointPassed(RouteMarkType::Start, 0);
      else if (passedCheckpointIdx + 1 == pointsCount)
        OnRoutePointPassed(RouteMarkType::Finish, 0);
      else
        OnRoutePointPassed(RouteMarkType::Intermediate, passedCheckpointIdx - 1);
    });
  });

  m_routingSession.SetSpeedCamShowCallback([this](m2::PointD const & point, double cameraSpeedKmPH)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, point, cameraSpeedKmPH]()
    {
      if (m_routeSpeedCamShowCallback)
        m_routeSpeedCamShowCallback(point, cameraSpeedKmPH);

      auto editSession = m_bmManager->GetEditSession();
      auto mark = editSession.CreateUserMark<SpeedCameraMark>(point);

      mark->SetIndex(0);
      if (cameraSpeedKmPH == SpeedCameraOnRoute::kNoSpeedInfo)
        return;

      double speed = cameraSpeedKmPH;
      if (measurement_utils::GetMeasurementUnits() == measurement_utils::Units::Imperial)
        speed = measurement_utils::KmphToMiph(cameraSpeedKmPH);

      mark->SetTitle(strings::to_string(static_cast<int>(speed + 0.5)));
    });
  });

  m_routingSession.SetSpeedCamClearCallback([this]()
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this]()
    {
      m_bmManager->GetEditSession().ClearGroup(UserMark::Type::SPEED_CAM);
      if (m_routeSpeedCamsClearCallback)
        m_routeSpeedCamsClearCallback();
    });
  });
}

void RoutingManager::SetBookmarkManager(BookmarkManager * bmManager)
{
  m_bmManager = bmManager;
}

void RoutingManager::SetTransitManager(TransitReadManager * transitManager)
{
  m_transitReadManager = transitManager;
}

void RoutingManager::OnBuildRouteReady(Route const & route, RouterResultCode code)
{
  // @TODO(bykoianko) Remove |code| from callback signature.
  CHECK_EQUAL(code, RouterResultCode::NoError, ());
  HidePreviewSegments();

  auto const hasWarnings = InsertRoute(route);
  m_drapeEngine.SafeCall(&df::DrapeEngine::StopLocationFollow);

  // Validate route (in case of bicycle routing it can be invalid).
  ASSERT(route.IsValid(), ());
  if (route.IsValid() && m_currentRouterType != routing::RouterType::Ruler)
  {
    m2::RectD routeRect = route.GetPoly().GetLimitRect();
    routeRect.Scale(kRouteScaleMultiplier);
    m_drapeEngine.SafeCall(&df::DrapeEngine::SetModelViewRect, routeRect,
                           true /* applyRotation */, -1 /* zoom */, true /* isAnim */,
                           true /* useVisibleViewport */);
  }

  CallRouteBuilded(hasWarnings ? RouterResultCode::HasWarnings : code, storage::CountriesSet());
}

void RoutingManager::OnRebuildRouteReady(Route const & route, RouterResultCode code)
{
  HidePreviewSegments();

  if (code != RouterResultCode::NoError)
    return;

  auto const hasWarnings = InsertRoute(route);
  CallRouteBuilded(hasWarnings ? RouterResultCode::HasWarnings : code, storage::CountriesSet());
}

void RoutingManager::OnNeedMoreMaps(uint64_t routeId, storage::CountriesSet const & absentCountries)
{
  // No need to inform user about maps needed for the route if the method is called
  // when RoutingSession contains a new route.
  if (m_routingSession.IsRouteValid() && !m_routingSession.IsRouteId(routeId))
    return;

  HidePreviewSegments();
  CallRouteBuilded(RouterResultCode::NeedMoreMaps, absentCountries);
}

void RoutingManager::OnRemoveRoute(routing::RouterResultCode code)
{
  HidePreviewSegments();
  RemoveRoute(true /* deactivateFollowing */);
  CallRouteBuilded(code, storage::CountriesSet());
}

void RoutingManager::OnRoutePointPassed(RouteMarkType type, size_t intermediateIndex)
{
  // Remove route point.
  ASSERT(m_bmManager != nullptr, ());
  RoutePointsLayout routePoints(*m_bmManager);
  routePoints.PassRoutePoint(type, intermediateIndex);

  if (type == RouteMarkType::Finish)
    RemoveRoute(false /* deactivateFollowing */);

  SaveRoutePoints();
}

void RoutingManager::OnLocationUpdate(location::GpsInfo const & info)
{
  m_extrapolator.OnLocationUpdate(info);
}

RouterType RoutingManager::GetBestRouter(m2::PointD const & startPoint,
                                         m2::PointD const & finalPoint) const
{
  // todo Implement something more sophisticated here (or delete the method).
  return GetLastUsedRouter();
}

RouterType RoutingManager::GetLastUsedRouter() const
{
  string routerTypeStr;
  if (!settings::Get(kRouterTypeKey, routerTypeStr))
    return RouterType::Vehicle;

  auto const routerType = FromString(routerTypeStr);

  switch (routerType)
  {
  case RouterType::Pedestrian:
  case RouterType::Bicycle:
  case RouterType::Transit:
  case RouterType::Ruler: return routerType;
  default: return RouterType::Vehicle;
  }
}

void RoutingManager::SetRouterImpl(RouterType type)
{
  auto const dataSourceGetterFn = m_callbacks.m_dataSourceGetter;
  CHECK(dataSourceGetterFn, ("Type:", type));

  VehicleType const vehicleType = GetVehicleType(type);

  m_loadAltitudes = vehicleType != VehicleType::Car;

  auto const countryFileGetter = [this](m2::PointD const & p) -> string {
    // TODO (@gorshenin): fix CountryInfoGetter to return CountryFile
    // instances instead of plain strings.
    return m_callbacks.m_countryInfoGetter().GetRegionCountryId(p);
  };

  auto numMwmIds = make_shared<NumMwmIds>();
  m_delegate.RegisterCountryFilesOnRoute(numMwmIds);

  auto & dataSource = m_callbacks.m_dataSourceGetter();

  auto localFileChecker = [this](string const & countryFile) -> bool {
    MwmSet::MwmId const mwmId = m_callbacks.m_dataSourceGetter().GetMwmIdByCountryFile(
      platform::CountryFile(countryFile));
    return mwmId.IsAlive();
  };

  auto const getMwmRectByName = [this](string const & countryId) -> m2::RectD {
    return m_callbacks.m_countryInfoGetter().GetLimitRectForLeaf(countryId);
  };

  auto regionsFinder =
      make_unique<AbsentRegionsFinder>(countryFileGetter, localFileChecker, numMwmIds, dataSource);

  std::unique_ptr<IRouter> router;
  if (type == RouterType::Ruler)
    router = make_unique<RulerRouter>();
  else
    router = make_unique<IndexRouter>(vehicleType, m_loadAltitudes, m_callbacks.m_countryParentNameGetterFn,
                                         countryFileGetter, getMwmRectByName, numMwmIds,
                                         MakeNumMwmTree(*numMwmIds, m_callbacks.m_countryInfoGetter()),
                                         m_routingSession, dataSource);

  m_routingSession.SetRoutingSettings(GetRoutingSettings(vehicleType));
  m_routingSession.SetRouter(std::move(router), std::move(regionsFinder));
  m_currentRouterType = type;
}

void RoutingManager::RemoveRoute(bool deactivateFollowing)
{
  GetPlatform().RunTask(Platform::Thread::Gui, [this, deactivateFollowing]()
  {
    {
      auto es = m_bmManager->GetEditSession();
      es.ClearGroup(UserMark::Type::TRANSIT);
      es.ClearGroup(UserMark::Type::SPEED_CAM);
      es.ClearGroup(UserMark::Type::ROAD_WARNING);
    }
    if (deactivateFollowing)
      SetPointsFollowingMode(false /* enabled */);
  });

  if (deactivateFollowing)
  {
    m_transitReadManager->BlockTransitSchemeMode(false /* isBlocked */);
    // Remove all subroutes.
    m_drapeEngine.SafeCall(&df::DrapeEngine::RemoveSubroute,
                           dp::DrapeID(), true /* deactivateFollowing */);
    m_drapeEngine.SafeCall(&df::DrapeEngine::SetSpeedLimitInfo, -1.f);
  }
  else
  {
    df::DrapeEngineLockGuard lock(m_drapeEngine);
    if (lock)
    {
      lock_guard<mutex> lockSubroutes(m_drapeSubroutesMutex);
      for (auto const & subrouteId : m_drapeSubroutes)
        lock.Get()->RemoveSubroute(subrouteId, false /* deactivateFollowing */);
    }
  }

  {
    lock_guard<mutex> lock(m_drapeSubroutesMutex);
    m_drapeSubroutes.clear();
    m_transitRouteInfo = TransitRouteInfo();
  }
}

void RoutingManager::CollectRoadWarnings(vector<routing::RouteSegment> const & segments,
                                         m2::PointD const & startPt, double baseDistance,
                                         GetMwmIdFn const & getMwmIdFn, RoadWarningsCollection & roadWarnings)
{
  auto const isWarnedType = [](RoutingOptions::Road roadType)
  {
    return (roadType == RoutingOptions::Road::Toll || roadType == RoutingOptions::Road::Ferry ||
            roadType == RoutingOptions::Road::Dirty);
  };

  bool const isCarRouter = (m_currentRouterType == RouterType::Vehicle);

  double currentDistance = baseDistance;
  double startDistance = baseDistance;
  RoutingOptions::Road lastType = RoutingOptions::Road::Usual;
  for (size_t i = 0; i < segments.size(); ++i)
  {
    auto const currentType = ChooseMainRoutingOptionRoad(segments[i].GetRoadTypes(), isCarRouter);
    if (currentType != lastType)
    {
      if (isWarnedType(lastType))
      {
        ASSERT(!roadWarnings[lastType].empty(), ());
        roadWarnings[lastType].back().m_distance = segments[i].GetDistFromBeginningMeters() - startDistance;
      }

      if (isWarnedType(currentType))
      {
        startDistance = currentDistance;
        auto const featureId = FeatureID(getMwmIdFn(segments[i].GetSegment().GetMwmId()),
                                         segments[i].GetSegment().GetFeatureId());
        auto const markPoint = i == 0 ? startPt : segments[i - 1].GetJunction().GetPoint();
        roadWarnings[currentType].push_back(RoadInfo(markPoint, featureId));
      }
      lastType = currentType;
    }
    currentDistance = segments[i].GetDistFromBeginningMeters();
  }
  if (isWarnedType(lastType))
    roadWarnings[lastType].back().m_distance = segments.back().GetDistFromBeginningMeters() - startDistance;
}

void RoutingManager::CreateRoadWarningMarks(RoadWarningsCollection && roadWarnings)
{
  if (roadWarnings.empty())
    return;

  GetPlatform().RunTask(Platform::Thread::Gui, [this, roadWarnings = std::move(roadWarnings)]()
  {
    auto es = m_bmManager->GetEditSession();
    for (auto const & typeInfo : roadWarnings)
    {
      auto const type = GetRoadType(typeInfo.first);
      for (size_t i = 0; i < typeInfo.second.size(); ++i)
      {
        auto const & routeInfo = typeInfo.second[i];
        auto mark = es.CreateUserMark<RoadWarningMark>(routeInfo.m_startPoint);
        mark->SetIndex(static_cast<uint32_t>(i));
        mark->SetRoadWarningType(type);
        mark->SetFeatureId(routeInfo.m_featureId);
        std::string distanceStr = platform::Distance::CreateFormatted(routeInfo.m_distance).ToString();
        mark->SetDistance(distanceStr);
      }
    }
  });
}

bool RoutingManager::InsertRoute(Route const & route)
{
  if (!m_drapeEngine)
    return false;

  // TODO: Now we always update whole route, so we need to remove previous one.
  RemoveRoute(false /* deactivateFollowing */);

  auto numMwmIds = make_shared<NumMwmIds>();
  m_delegate.RegisterCountryFilesOnRoute(numMwmIds);
  auto const getMwmId = [this, numMwmIds](routing::NumMwmId numMwmId)
  {
    return m_callbacks.m_dataSourceGetter().GetMwmIdByCountryFile(numMwmIds->GetFile(numMwmId));
  };

  RoadWarningsCollection roadWarnings;

  bool const isTransitRoute = (m_currentRouterType == RouterType::Transit);
  shared_ptr<TransitRouteDisplay> transitRouteDisplay;
  if (isTransitRoute)
  {
    transitRouteDisplay = make_shared<TransitRouteDisplay>(*m_transitReadManager, getMwmId,
                                                           m_callbacks.m_stringsBundleGetter,
                                                           m_bmManager, m_transitSymbolSizes);
  }

  vector<RouteSegment> segments;
  double distance = 0.0;
  auto const subroutesCount = route.GetSubrouteCount();
  for (size_t subrouteIndex = route.GetCurrentSubrouteIdx(); subrouteIndex < subroutesCount; ++subrouteIndex)
  {
    route.GetSubrouteInfo(subrouteIndex, segments);

    auto const startPt = route.GetSubrouteAttrs(subrouteIndex).GetStart().GetPoint();
    auto subroute = CreateDrapeSubroute(segments, startPt, distance,
                                        static_cast<double>(subroutesCount - subrouteIndex - 1),
                                        m_currentRouterType);
    if (!subroute)
      continue;
    distance = segments.back().GetDistFromBeginningMerc();
    switch (m_currentRouterType)
    {
      case RouterType::Vehicle:
        {
          subroute->m_routeType = df::RouteType::Car;
          subroute->AddStyle(df::SubrouteStyle(df::kRouteColor, df::kRouteOutlineColor));
          FillTrafficForRendering(segments, subroute->m_traffic);
          FillTurnsDistancesForRendering(segments, subroute->m_baseDistance, subroute->m_turns);
          break;
        }
      case RouterType::Transit:
        {
          subroute->m_routeType = df::RouteType::Transit;
          if (!transitRouteDisplay->ProcessSubroute(segments, *subroute.get()))
            continue;
          break;
        }
      case RouterType::Pedestrian:
        {
          subroute->m_routeType = df::RouteType::Pedestrian;
          subroute->AddStyle(df::SubrouteStyle(df::kRoutePedestrian, df::RoutePattern(4.0, 2.0)));
          break;
        }
      case RouterType::Bicycle:
        {
          subroute->m_routeType = df::RouteType::Bicycle;
          subroute->AddStyle(df::SubrouteStyle(df::kRouteBicycle, df::RoutePattern(8.0, 2.0)));
          FillTurnsDistancesForRendering(segments, subroute->m_baseDistance, subroute->m_turns);
          break;
        }
      case RouterType::Ruler:
        {
          subroute->m_routeType = df::RouteType::Ruler;
          subroute->AddStyle(df::SubrouteStyle(df::kRouteRuler, df::RoutePattern(16.0, 2.0)));
          break;
        }
      default: CHECK(false, ("Unknown router type"));
    }

    CollectRoadWarnings(segments, startPt, subroute->m_baseDistance, getMwmId, roadWarnings);

    auto const subrouteId = m_drapeEngine.SafeCallWithResult(&df::DrapeEngine::AddSubroute,
                                                             df::SubrouteConstPtr(subroute.release()));

    // TODO: we will send subrouteId to routing subsystem when we can partly update route.
    //route.SetSubrouteUid(subrouteIndex, static_cast<SubrouteUid>(subrouteId));
    lock_guard<mutex> lock(m_drapeSubroutesMutex);
    m_drapeSubroutes.push_back(subrouteId);
  }

  {
    lock_guard<mutex> lock(m_drapeSubroutesMutex);
    m_transitRouteInfo = isTransitRoute ? transitRouteDisplay->GetRouteInfo() : TransitRouteInfo();
  }

  if (isTransitRoute)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [transitRouteDisplay = std::move(transitRouteDisplay)]()
    {
      transitRouteDisplay->CreateTransitMarks();
    });
  }

  bool const hasWarnings = !roadWarnings.empty();
  if (hasWarnings && m_currentRouterType == RouterType::Vehicle)
    CreateRoadWarningMarks(std::move(roadWarnings));

  return hasWarnings;
}

void RoutingManager::FollowRoute()
{
  if (!m_routingSession.EnableFollowMode())
    return;

  m_transitReadManager->BlockTransitSchemeMode(true /* isBlocked */);

  // Switching on the extrapolator only for following mode in car and bicycle navigation.
  m_extrapolator.Enable(m_currentRouterType == RouterType::Vehicle ||
                        m_currentRouterType == RouterType::Bicycle);
  m_delegate.OnRouteFollow(m_currentRouterType);

  m_bmManager->GetEditSession().ClearGroup(UserMark::Type::ROAD_WARNING);
  HideRoutePoint(RouteMarkType::Start);
  SetPointsFollowingMode(true /* enabled */);

  CancelRecommendation(Recommendation::RebuildAfterPointsLoading);
}

void RoutingManager::CloseRouting(bool removeRoutePoints)
{
  m_extrapolator.Enable(false);
  // Hide preview.
  HidePreviewSegments();

  if (m_routingSession.IsBuilt())
    m_routingSession.EmitCloseRoutingEvent();
  m_routingSession.Reset();
  RemoveRoute(true /* deactivateFollowing */);

  if (removeRoutePoints)
  {
    m_bmManager->GetEditSession().ClearGroup(UserMark::Type::ROUTING);
    CancelRecommendation(Recommendation::RebuildAfterPointsLoading);
  }
}

void RoutingManager::GetRouteFollowingInfo(routing::FollowingInfo & info)
{
  m_routingSession.GetRouteFollowingInfo(info);
  m_drapeEngine.SafeCall(&df::DrapeEngine::SetSpeedLimitInfo, info.m_speedLimitMps);
}

void RoutingManager::SetLastUsedRouter(RouterType type)
{
  settings::Set(kRouterTypeKey, ToString(type));
}

void RoutingManager::HideRoutePoint(RouteMarkType type, size_t intermediateIndex)
{
  RoutePointsLayout routePoints(*m_bmManager);
  RouteMarkPoint * mark = routePoints.GetRoutePointForEdit(type, intermediateIndex);
  if (mark != nullptr)
    mark->SetIsVisible(false);
}

bool RoutingManager::IsMyPosition(RouteMarkType type, size_t intermediateIndex)
{
  RoutePointsLayout routePoints(*m_bmManager);
  RouteMarkPoint const * mark = routePoints.GetRoutePoint(type, intermediateIndex);
  return mark != nullptr ? mark->IsMyPosition() : false;
}

vector<RouteMarkData> RoutingManager::GetRoutePoints() const
{
  vector<RouteMarkData> result;
  RoutePointsLayout routePoints(*m_bmManager);
  for (auto const & p : routePoints.GetRoutePoints())
    result.push_back(p->GetMarkData());
  return result;
}

size_t RoutingManager::GetRoutePointsCount() const
{
  RoutePointsLayout routePoints(*m_bmManager);
  return routePoints.GetRoutePointsCount();
}

bool RoutingManager::CouldAddIntermediatePoint() const
{
  if (!IsRoutingActive())
    return false;

  return m_bmManager->GetUserMarkIds(UserMark::Type::ROUTING).size()
    < RoutePointsLayout::kMaxIntermediatePointsCount + 2;
}

void RoutingManager::AddRoutePoint(RouteMarkData && markData)
{
  ASSERT(m_bmManager != nullptr, ());
  RoutePointsLayout routePoints(*m_bmManager);

  // Always replace start and finish points.
  if (markData.m_pointType == RouteMarkType::Start || markData.m_pointType == RouteMarkType::Finish)
    routePoints.RemoveRoutePoint(markData.m_pointType);

  if (markData.m_isMyPosition)
  {
    RouteMarkPoint const * mark = routePoints.GetMyPositionPoint();
    if (mark != nullptr)
      routePoints.RemoveRoutePoint(mark->GetRoutePointType(), mark->GetIntermediateIndex());
  }

  markData.m_isVisible = !markData.m_isMyPosition;
  routePoints.AddRoutePoint(std::move(markData));
  ReorderIntermediatePoints();
}

void RoutingManager::ContinueRouteToPoint(RouteMarkData && markData)
{
  ASSERT(m_bmManager != nullptr, ());
  ASSERT(markData.m_pointType == RouteMarkType::Finish, ("New route point should have type RouteMarkType::Finish"));
  RoutePointsLayout routePoints(*m_bmManager);

  // Finish point is now Intermediate point
  RouteMarkPoint * finishMarkData = routePoints.GetRoutePointForEdit(RouteMarkType::Finish);
  finishMarkData->SetRoutePointType(RouteMarkType::Intermediate);
  finishMarkData->SetIntermediateIndex(routePoints.GetRoutePointsCount() - 2);

  if (markData.m_isMyPosition)
  {
    RouteMarkPoint const * mark = routePoints.GetMyPositionPoint();
    if (mark)
      routePoints.RemoveRoutePoint(mark->GetRoutePointType(), mark->GetIntermediateIndex());
  }

  markData.m_intermediateIndex = routePoints.GetRoutePointsCount() - 1;
  markData.m_isVisible = !markData.m_isMyPosition;
  routePoints.AddRoutePoint(std::move(markData));
}

void RoutingManager::RemoveRoutePoint(RouteMarkType type, size_t intermediateIndex)
{
  ASSERT(m_bmManager != nullptr, ());
  RoutePointsLayout routePoints(*m_bmManager);
  routePoints.RemoveRoutePoint(type, intermediateIndex);
}

void RoutingManager::RemoveRoutePoints()
{
  ASSERT(m_bmManager != nullptr, ());
  RoutePointsLayout routePoints(*m_bmManager);
  routePoints.RemoveRoutePoints();
}

void RoutingManager::RemoveIntermediateRoutePoints()
{
  ASSERT(m_bmManager != nullptr, ());
  RoutePointsLayout routePoints(*m_bmManager);
  routePoints.RemoveIntermediateRoutePoints();
}

void RoutingManager::MoveRoutePoint(RouteMarkType currentType, size_t currentIntermediateIndex,
                                    RouteMarkType targetType, size_t targetIntermediateIndex)
{
  ASSERT(m_bmManager != nullptr, ());
  RoutePointsLayout routePoints(*m_bmManager);
  routePoints.MoveRoutePoint(currentType, currentIntermediateIndex,
                             targetType, targetIntermediateIndex);
}

void RoutingManager::MoveRoutePoint(size_t currentIndex, size_t targetIndex)
{
  ASSERT(m_bmManager != nullptr, ());

  RoutePointsLayout routePoints(*m_bmManager);
  size_t const sz = routePoints.GetRoutePointsCount();
  auto const convertIndex = [sz](RouteMarkType & type, size_t & index) {
    if (index == 0)
    {
      type = RouteMarkType::Start;
      index = 0;
    }
    else if (index + 1 == sz)
    {
      type = RouteMarkType::Finish;
      index = 0;
    }
    else
    {
      type = RouteMarkType::Intermediate;
      --index;
    }
  };
  RouteMarkType currentType;
  RouteMarkType targetType;

  convertIndex(currentType, currentIndex);
  convertIndex(targetType, targetIndex);

  routePoints.MoveRoutePoint(currentType, currentIndex, targetType, targetIndex);
}

void RoutingManager::SetPointsFollowingMode(bool enabled)
{
  ASSERT(m_bmManager != nullptr, ());
  RoutePointsLayout routePoints(*m_bmManager);
  routePoints.SetFollowingMode(enabled);
}

void RoutingManager::ReorderIntermediatePoints()
{
  vector<RouteMarkPoint *> prevPoints;
  vector<m2::PointD> prevPositions;
  prevPoints.reserve(RoutePointsLayout::kMaxIntermediatePointsCount);
  prevPositions.reserve(RoutePointsLayout::kMaxIntermediatePointsCount);
  RoutePointsLayout routePoints(*m_bmManager);

  RouteMarkPoint * addedPoint = nullptr;
  m2::PointD addedPosition;

  for (auto const & p : routePoints.GetRoutePoints())
  {
    CHECK(p, ());
    if (p->GetRoutePointType() == RouteMarkType::Intermediate)
    {
      // Note. An added (new) intermediate point is the first intermediate point at |routePoints.GetRoutePoints()|.
      // The other intermediate points are former ones.
      if (addedPoint == nullptr)
      {
        addedPoint = p;
        addedPosition = p->GetPivot();
      }
      else
      {
        prevPoints.push_back(p);
        prevPositions.push_back(p->GetPivot());
      }
    }
  }
  if (addedPoint == nullptr)
    return;

  CheckpointPredictor predictor(m_routingSession.GetStartPoint(), m_routingSession.GetEndPoint());

  size_t const insertIndex = predictor.PredictPosition(prevPositions, addedPosition);
  addedPoint->SetIntermediateIndex(insertIndex);
  for (size_t i = 0; i < prevPoints.size(); ++i)
    prevPoints[i]->SetIntermediateIndex(i < insertIndex ? i : i + 1);
}

void RoutingManager::GenerateNotifications(vector<string> & turnNotifications)
{
  m_routingSession.GenerateNotifications(turnNotifications);
}

void RoutingManager::BuildRoute(uint32_t timeoutSec)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ("BuildRoute"));

  m_bmManager->GetEditSession().ClearGroup(UserMark::Type::TRANSIT);

  auto routePoints = GetRoutePoints();
  if (routePoints.size() < 2)
  {
    CallRouteBuilded(RouterResultCode::Cancelled, storage::CountriesSet());
    CloseRouting(false /* remove route points */);
    return;
  }

  // Update my position.
  for (auto & p : routePoints)
  {
    if (!p.m_isMyPosition)
      continue;

    auto const & myPosition = m_bmManager->MyPositionMark();
    if (!myPosition.HasPosition())
    {
      CallRouteBuilded(RouterResultCode::NoCurrentPosition, storage::CountriesSet());
      return;
    }
    p.m_position = myPosition.GetPivot();
  }

  // Check for equal points.
  for (size_t i = 0; i < routePoints.size(); i++)
  {
    for (size_t j = i + 1; j < routePoints.size(); j++)
    {
      if (routePoints[i].m_position.EqualDxDy(routePoints[j].m_position, mercator::kPointEqualityEps))
      {
        CallRouteBuilded(RouterResultCode::Cancelled, storage::CountriesSet());
        CloseRouting(false /* remove route points */);
        return;
      }
    }
  }

  if (IsRoutingActive())
    CloseRouting(false /* remove route points */);

  ShowPreviewSegments(routePoints);

  // Route points preview.
  // Disabled preview zoom to fix https://github.com/organicmaps/organicmaps/issues/5409.
  // Uncomment next lines to enable back zoom on route point add/remove.

  //m2::RectD rect = ShowPreviewSegments(routePoints);
  //rect.Scale(kRouteScaleMultiplier);
  //m_drapeEngine.SafeCall(&df::DrapeEngine::SetModelViewRect, rect, true /* applyRotation */,
  //                       -1 /* zoom */, true /* isAnim */, true /* useVisibleViewport */);

  m_routingSession.ClearPositionAccumulator();
  m_routingSession.SetUserCurrentPosition(routePoints.front().m_position);

  vector<m2::PointD> points;
  points.reserve(routePoints.size());
  for (auto const & point : routePoints)
    points.push_back(point.m_position);

  m_routingSession.BuildRoute(Checkpoints(std::move(points)), timeoutSec);
}

void RoutingManager::SetUserCurrentPosition(m2::PointD const & position)
{
  m_routingSession.PushPositionAccumulator(position);

  if (IsRoutingActive())
    m_routingSession.SetUserCurrentPosition(position);

  if (m_routeRecommendCallback != nullptr)
  {
    // Check if we've found my position almost immediately after route points loading.
    auto constexpr kFoundLocationInterval = 2.0;
    auto const elapsed = chrono::steady_clock::now() - m_loadRoutePointsTimestamp;
    auto const sec = chrono::duration_cast<chrono::duration<double>>(elapsed).count();
    if (sec <= kFoundLocationInterval)
    {
      m_routeRecommendCallback(Recommendation::RebuildAfterPointsLoading);
      CancelRecommendation(Recommendation::RebuildAfterPointsLoading);
    }
  }
}

bool RoutingManager::DisableFollowMode()
{
  bool const disabled = m_routingSession.DisableFollowMode();
  if (disabled)
  {
    m_transitReadManager->BlockTransitSchemeMode(false /* isBlocked */);
    m_drapeEngine.SafeCall(&df::DrapeEngine::DeactivateRouteFollowing);
  }
  return disabled;
}

void RoutingManager::CheckLocationForRouting(location::GpsInfo const & info)
{
  if (!IsRoutingActive())
    return;

  SessionState const state = m_routingSession.OnLocationPositionChanged(info);
  if (state == SessionState::RouteNeedRebuild)
  {
    m_routingSession.RebuildRoute(
        mercator::FromLatLon(info.m_latitude, info.m_longitude),
        [this](Route const & route, RouterResultCode code) { OnRebuildRouteReady(route, code); },
        nullptr /* needMoreMapsCallback */, nullptr /* removeRouteCallback */,
        RouterDelegate::kNoTimeout, SessionState::RouteRebuilding, true /* adjustToPrevRoute */);
  }
}

void RoutingManager::CallRouteBuilded(RouterResultCode code,
                                      storage::CountriesSet const & absentCountries)
{
  m_routingBuildingCallback(code, absentCountries);
}

void RoutingManager::MatchLocationToRoute(location::GpsInfo & location,
                                          location::RouteMatchingInfo & routeMatchingInfo)
{
  if (!IsRoutingActive())
    return;

  bool const matchedToRoute = m_routingSession.MatchLocationToRoute(location, routeMatchingInfo);

  if (!matchedToRoute && m_currentRouterType == RouterType::Vehicle)
    m_routingSession.MatchLocationToRoadGraph(location);
}

location::RouteMatchingInfo RoutingManager::GetRouteMatchingInfo(location::GpsInfo & info)
{
  CheckLocationForRouting(info);

  location::RouteMatchingInfo routeMatchingInfo;
  MatchLocationToRoute(info, routeMatchingInfo);
  return routeMatchingInfo;
}

void RoutingManager::SetDrapeEngine(ref_ptr<df::DrapeEngine> engine, bool is3dAllowed)
{
  m_drapeEngine.Set(engine);
  if (engine == nullptr)
    return;

  // Apply gps info which was set before drape engine creation.
  if (m_gpsInfoCache != nullptr)
  {
    auto routeMatchingInfo = GetRouteMatchingInfo(*m_gpsInfoCache);
    m_drapeEngine.SafeCall(&df::DrapeEngine::SetGpsInfo, *m_gpsInfoCache,
                           m_routingSession.IsNavigable(), routeMatchingInfo);
    m_gpsInfoCache.reset();
  }

  vector<string> symbols;
  symbols.reserve(kTransitSymbols.size() * 2);
  for (auto const & typePair : kTransitSymbols)
  {
    symbols.push_back(typePair.second + "-s");
    symbols.push_back(typePair.second + "-m");
  }
  m_drapeEngine.SafeCall(&df::DrapeEngine::RequestSymbolsSize, symbols,
                         [this, is3dAllowed](map<string, m2::PointF> && sizes)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, is3dAllowed, sizes = std::move(sizes)]() mutable
    {
      m_transitSymbolSizes = std::move(sizes);

      // In case of the engine reinitialization recover route.
      if (IsRoutingActive())
      {
        m_routingSession.RouteCall([this](Route const & route) { InsertRoute(route); });

        if (is3dAllowed && m_routingSession.IsFollowing())
          m_drapeEngine.SafeCall(&df::DrapeEngine::EnablePerspective);
      }
    });
  });
}

bool RoutingManager::HasRouteAltitude() const
{
  return m_loadAltitudes && m_routingSession.HasRouteAltitude();
}

bool RoutingManager::GetRouteAltitudesAndDistancesM(DistanceAltitude & da) const
{
  return m_routingSession.GetRouteAltitudesAndDistancesM(da.m_distances, da.m_altitudes);
}

void RoutingManager::DistanceAltitude::Simplify(double altitudeDeviation)
{
  class IterT
  {
    DistanceAltitude const & m_da;
    size_t m_ind = 0;

  public:
    IterT(DistanceAltitude const & da, bool isBeg) : m_da(da)
    {
      m_ind = isBeg ? 0 : m_da.GetSize();
    }

    IterT(IterT const & rhs) = default;
    IterT & operator=(IterT const & rhs) { m_ind = rhs.m_ind; return *this; }

    bool operator!=(IterT const & rhs) const { return m_ind != rhs.m_ind; }

    IterT & operator++() { ++m_ind; return *this; }
    IterT operator+(size_t inc) const
    {
      IterT res = *this;
      res.m_ind += inc;
      return res;
    }
    int64_t operator-(IterT const & rhs) const { return int64_t(m_ind) - int64_t(rhs.m_ind); }

    m2::PointD operator*() const { return { m_da.m_distances[m_ind], double(m_da.m_altitudes[m_ind]) }; }
  };

  std::vector<m2::PointD> out;

  // 1. Deviation from approximated altitude.
//  double constexpr eps = 1.415; // ~sqrt(2)
//  struct DeviationFromApproxY
//  {
//    double operator()(m2::PointD const & a, m2::PointD const & b, m2::PointD const & x) const
//    {
//      double f = (x.x - a.x) / (b.x - a.x);
//      ASSERT(0 <= f && f <= 1, (f));  // distance is an icreasing function
//      double const approxY = (1 - f) * a.y + f * b.y;
//      return fabs(approxY - x.y);
//    }
//  } distFn;
//  SimplifyNearOptimal(20 /* maxFalseLookAhead */, IterT(*this, true), IterT(*this, false),
//                      eps, distFn, AccumulateSkipSmallTrg(distFn, out, eps));

  // 2. Default square distance from segment.
  SimplifyDefault(IterT(*this, true), IterT(*this, false), base::Pow2(altitudeDeviation), out);

  size_t const count = out.size();
  m_distances.resize(count);
  m_altitudes.resize(count);
  for (size_t i = 0; i < count; ++i)
  {
    m_distances[i] = out[i].x;
    m_altitudes[i] = geometry::Altitude(out[i].y);
  }
}

bool RoutingManager::DistanceAltitude::GenerateRouteAltitudeChart(
      uint32_t width, uint32_t height, vector<uint8_t> & imageRGBAData) const
{
  if (GetSize() == 0)
    return false;

  return maps::GenerateChart(width, height, m_distances, m_altitudes, GetStyleReader().GetCurrentStyle(), imageRGBAData);
}

void RoutingManager::DistanceAltitude::CalculateAscentDescent(uint32_t & totalAscentM, uint32_t & totalDescentM) const
{
  totalAscentM = 0;
  totalDescentM = 0;
  for (size_t i = 1; i < m_altitudes.size(); i++)
  {
    int16_t const delta = m_altitudes[i] - m_altitudes[i - 1];
    if (delta > 0)
      totalAscentM += delta;
    else
      totalDescentM += -delta;
  }
}

std::string DebugPrint(RoutingManager::DistanceAltitude const & da)
{
  return DebugPrint(da.m_altitudes);
}

void RoutingManager::SetRouter(RouterType type)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ("SetRouter"));

  if (m_currentRouterType == type)
    return;

  // Hide preview.
  HidePreviewSegments();

  SetLastUsedRouter(type);
  SetRouterImpl(type);
}

// static
uint32_t RoutingManager::InvalidRoutePointsTransactionId()
{
  return kInvalidTransactionId;
}

uint32_t RoutingManager::GenerateRoutePointsTransactionId() const
{
  static uint32_t id = kInvalidTransactionId + 1;
  return id++;
}

uint32_t RoutingManager::OpenRoutePointsTransaction()
{
  auto const id = GenerateRoutePointsTransactionId();
  m_routePointsTransactions[id].m_routeMarks = GetRoutePoints();
  return id;
}

void RoutingManager::ApplyRoutePointsTransaction(uint32_t transactionId)
{
  if (m_routePointsTransactions.find(transactionId) == m_routePointsTransactions.end())
    return;

  // If we apply a transaction we can remove all earlier transactions.
  // All older transactions must be kept since they can be applied or cancelled later.
  for (auto it = m_routePointsTransactions.begin(); it != m_routePointsTransactions.end();)
  {
    if (it->first <= transactionId)
      it = m_routePointsTransactions.erase(it);
    else
      ++it;
  }
}

void RoutingManager::CancelRoutePointsTransaction(uint32_t transactionId)
{
  auto const it = m_routePointsTransactions.find(transactionId);
  if (it == m_routePointsTransactions.end())
    return;
  auto routeMarks = it->second.m_routeMarks;

  // If we cancel a transaction we must remove all later transactions.
  for (auto it = m_routePointsTransactions.begin(); it != m_routePointsTransactions.end();)
  {
    if (it->first >= transactionId)
      it = m_routePointsTransactions.erase(it);
    else
      ++it;
  }

  // Revert route points.
  ASSERT(m_bmManager != nullptr, ());
  auto editSession = m_bmManager->GetEditSession();
  editSession.ClearGroup(UserMark::Type::ROUTING);
  RoutePointsLayout routePoints(*m_bmManager);
  for (auto & markData : routeMarks)
    routePoints.AddRoutePoint(std::move(markData));
}

bool RoutingManager::HasSavedRoutePoints() const
{
  auto const fileName = GetPlatform().SettingsPathForFile(kRoutePointsFile);
  return GetPlatform().IsFileExistsByFullPath(fileName);
}

void RoutingManager::LoadRoutePoints(LoadRouteHandler const & handler)
{
  GetPlatform().RunTask(Platform::Thread::File, [this, handler]()
  {
    if (!HasSavedRoutePoints())
    {
      if (handler)
        handler(false /* success */);
      return;
    }

    // Delete file after loading.
    auto const fileName = GetPlatform().SettingsPathForFile(kRoutePointsFile);
    SCOPE_GUARD(routePointsFileGuard, bind(&FileWriter::DeleteFileX, cref(fileName)));

    string data;
    try
    {
      ReaderPtr<Reader>(GetPlatform().GetReader(fileName)).ReadAsString(data);
    }
    catch (RootException const & ex)
    {
      LOG(LWARNING, ("Loading road points failed:", ex.Msg()));
      if (handler)
        handler(false /* success */);
      return;
    }

    auto points = DeserializeRoutePoints(data);
    if (handler && points.empty())
    {
      handler(false /* success */);
      return;
    }

    GetPlatform().RunTask(Platform::Thread::Gui,
                          [this, handler, points = std::move(points)]() mutable
    {
      ASSERT(m_bmManager != nullptr, ());
      // If we have found my position, we use my position as start point.
      auto const & myPosMark = m_bmManager->MyPositionMark();
      auto editSession = m_bmManager->GetEditSession();
      editSession.ClearGroup(UserMark::Type::ROUTING);
      for (auto & p : points)
      {
        if (p.m_pointType == RouteMarkType::Start && myPosMark.HasPosition())
        {
          RouteMarkData startPt;
          startPt.m_pointType = RouteMarkType::Start;
          startPt.m_isMyPosition = true;
          startPt.m_position = myPosMark.GetPivot();
          AddRoutePoint(std::move(startPt));
        }
        else
        {
          AddRoutePoint(std::move(p));
        }
      }

      // If we don't have my position, save loading timestamp. Probably
      // we will get my position soon.
      if (!myPosMark.HasPosition())
        m_loadRoutePointsTimestamp = chrono::steady_clock::now();

      if (handler)
        handler(true /* success */);
    });
  });
}

void RoutingManager::SaveRoutePoints()
{
  auto points = GetRoutePointsToSave();
  if (points.empty())
  {
    DeleteSavedRoutePoints();
    return;
  }

  GetPlatform().RunTask(Platform::Thread::File, [points = std::move(points)]()
  {
    try
    {
      auto const fileName = GetPlatform().SettingsPathForFile(kRoutePointsFile);
      FileWriter writer(fileName);
      string const pointsData = SerializeRoutePoints(points);
      writer.Write(pointsData.c_str(), pointsData.length());
    }
    catch (RootException const & ex)
    {
      LOG(LWARNING, ("Saving road points failed:", ex.Msg()));
    }
  });
}

vector<RouteMarkData> RoutingManager::GetRoutePointsToSave() const
{
  auto points = GetRoutePoints();
  if (points.size() < 2 || points.back().m_isPassed)
    return {};

  vector<RouteMarkData> result;
  result.reserve(points.size());

  // Save last passed point. It will be used on points loading if my position
  // isn't determined.
  result.emplace_back(GetLastPassedPoint(m_bmManager, points));

  for (auto & p : points)
  {
    // Here we skip passed points and the start point.
    if (p.m_isPassed || p.m_pointType == RouteMarkType::Start)
      continue;

    result.push_back(std::move(p));
  }

  if (result.size() < 2)
    return {};

  return result;
}

void RoutingManager::OnExtrapolatedLocationUpdate(location::GpsInfo const & info)
{
  location::GpsInfo gpsInfo(info);
  if (!m_drapeEngine)
    m_gpsInfoCache = make_unique<location::GpsInfo>(gpsInfo);

  auto routeMatchingInfo = GetRouteMatchingInfo(gpsInfo);
  m_drapeEngine.SafeCall(&df::DrapeEngine::SetGpsInfo, gpsInfo, m_routingSession.IsNavigable(),
                         routeMatchingInfo);
}

void RoutingManager::DeleteSavedRoutePoints()
{
  if (!HasSavedRoutePoints())
    return;

  GetPlatform().RunTask(Platform::Thread::File, []()
  {
    auto const fileName = GetPlatform().SettingsPathForFile(kRoutePointsFile);
    FileWriter::DeleteFileX(fileName);
  });
}

void RoutingManager::UpdatePreviewMode()
{
  SetSubroutesVisibility(false /* visible */);
  HidePreviewSegments();
  ShowPreviewSegments(GetRoutePoints());
}

void RoutingManager::CancelPreviewMode()
{
  SetSubroutesVisibility(true /* visible */);
  HidePreviewSegments();
}

m2::RectD RoutingManager::ShowPreviewSegments(vector<RouteMarkData> const & routePoints)
{
  df::DrapeEngineLockGuard lock(m_drapeEngine);
  if (!lock)
    return mercator::Bounds::FullRect();

  m2::RectD rect;
  for (size_t pointIndex = 0; pointIndex + 1 < routePoints.size(); pointIndex++)
  {
    rect.Add(routePoints[pointIndex].m_position);
    rect.Add(routePoints[pointIndex + 1].m_position);
    lock.Get()->AddRoutePreviewSegment(routePoints[pointIndex].m_position,
                                       routePoints[pointIndex + 1].m_position);
  }
  return rect;
}

void RoutingManager::HidePreviewSegments()
{
  m_drapeEngine.SafeCall(&df::DrapeEngine::RemoveAllRoutePreviewSegments);
}

void RoutingManager::CancelRecommendation(Recommendation recommendation)
{
  if (recommendation == Recommendation::RebuildAfterPointsLoading)
    m_loadRoutePointsTimestamp = chrono::steady_clock::time_point();
}

TransitRouteInfo RoutingManager::GetTransitRouteInfo() const
{
  lock_guard<mutex> lock(m_drapeSubroutesMutex);
  return m_transitRouteInfo;
}

void RoutingManager::SetSubroutesVisibility(bool visible)
{
  df::DrapeEngineLockGuard lock(m_drapeEngine);
  if (!lock)
    return;

  lock_guard<mutex> lockSubroutes(m_drapeSubroutesMutex);
  for (auto const & subrouteId : m_drapeSubroutes)
    lock.Get()->SetSubrouteVisibility(subrouteId, visible);
}

bool RoutingManager::IsSpeedCamLimitExceeded() const
{
  return m_routingSession.IsSpeedCamLimitExceeded();
}
