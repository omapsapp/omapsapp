#include "routing/index_router.hpp"

#include "routing/base/astar_progress.hpp"

#include "routing/car_directions.hpp"
#include "routing/fake_ending.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_loader.hpp"
#include "routing/index_graph_starter.hpp"
#include "routing/index_graph_starter_joints.hpp"
#include "routing/index_road_graph.hpp"
#include "routing/junction_visitor.hpp"
#include "routing/leaps_graph.hpp"
#include "routing/leaps_postprocessor.hpp"
#include "routing/mwm_hierarchy_handler.hpp"
#include "routing/pedestrian_directions.hpp"
#include "routing/route.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/routing_options.hpp"
#include "routing/single_vehicle_world_graph.hpp"
#include "routing/speed_camera_prohibition.hpp"
#include "routing/traffic_stash.hpp"
#include "routing/transit_world_graph.hpp"
#include "routing/vehicle_mask.hpp"

#include "transit/transit_entities.hpp"

#include "routing_common/bicycle_model.hpp"
#include "routing_common/car_model.hpp"
#include "routing_common/pedestrian_model.hpp"

#include "indexer/data_source.hpp"

#include "platform/settings.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"
#include "geometry/parametrized_segment.hpp"
#include "geometry/polyline2d.hpp"
#include "geometry/segment2d.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"

#include "defines.hpp"

#include <algorithm>
#include <deque>
#include <iterator>
#include <map>

namespace routing
{
using namespace std;

namespace
{
size_t constexpr kMaxRoadCandidates = 10;
uint32_t constexpr kVisitPeriodForLeaps = 10;
uint32_t constexpr kVisitPeriod = 40;

double constexpr kLeapsStageContribution = 0.15;
double constexpr kCandidatesStageContribution = 0.55;
double constexpr kAlmostZeroContribution = 1e-7;

// If user left the route within this range(meters), adjust the route. Else full rebuild.
double constexpr kAdjustRangeM = 5000.0;
// Full rebuild if distance(meters) is less.
double constexpr kMinDistanceToFinishM = 10000;
// Near MWMs criteria when choosing routing mode.
double constexpr kCloseMwmPointsDistanceM = 300000;

double CalcMaxSpeed(NumMwmIds const & numMwmIds,
                    VehicleModelFactoryInterface const & vehicleModelFactory,
                    VehicleType vehicleType)
{
  if (vehicleType == VehicleType::Transit)
    return kTransitMaxSpeedKMpH;

  double maxSpeed = 0.0;
  numMwmIds.ForEachId([&](NumMwmId id) {
    string const & country = numMwmIds.GetFile(id).GetName();
    double const mwmMaxSpeed =
        vehicleModelFactory.GetVehicleModelForCountry(country)->GetMaxWeightSpeed();
    maxSpeed = max(maxSpeed, mwmMaxSpeed);
  });
  CHECK_GREATER(maxSpeed, 0.0, ("Most likely |numMwmIds| is empty."));
  return maxSpeed;
}

SpeedKMpH const & CalcOffroadSpeed(VehicleModelFactoryInterface const & vehicleModelFactory)
{
  return vehicleModelFactory.GetVehicleModel()->GetOffroadSpeed();
}

shared_ptr<VehicleModelFactoryInterface> CreateVehicleModelFactory(
    VehicleType vehicleType, CountryParentNameGetterFn const & countryParentNameGetterFn)
{
  switch (vehicleType)
  {
  case VehicleType::Pedestrian:
  case VehicleType::Transit:
    return make_shared<PedestrianModelFactory>(countryParentNameGetterFn);
  case VehicleType::Bicycle: return make_shared<BicycleModelFactory>(countryParentNameGetterFn);
  case VehicleType::Car: return make_shared<CarModelFactory>(countryParentNameGetterFn);
  case VehicleType::Count:
    CHECK(false, ("Can't create VehicleModelFactoryInterface for", vehicleType));
    return nullptr;
  }
  UNREACHABLE();
}

unique_ptr<DirectionsEngine> CreateDirectionsEngine(VehicleType vehicleType,
                                                    shared_ptr<NumMwmIds> numMwmIds,
                                                    MwmDataSource & dataSource)
{
  switch (vehicleType)
  {
  case VehicleType::Pedestrian:
  case VehicleType::Transit: return make_unique<PedestrianDirectionsEngine>(dataSource, numMwmIds);
  case VehicleType::Bicycle:
  case VehicleType::Car: return make_unique<CarDirectionsEngine>(dataSource, numMwmIds);
  case VehicleType::Count:
    CHECK(false, ("Can't create DirectionsEngine for", vehicleType));
    return nullptr;
  }
  UNREACHABLE();
}

shared_ptr<TrafficStash> CreateTrafficStash(VehicleType, shared_ptr<NumMwmIds>, traffic::TrafficCache const &)
{
  return nullptr;
  //return (vehicleType == VehicleType::Car ? make_shared<TrafficStash>(trafficCache, numMwmIds) : nullptr);
}

void PushPassedSubroutes(Checkpoints const & checkpoints, vector<Route::SubrouteAttrs> & subroutes)
{
  for (size_t i = 0; i < checkpoints.GetPassedIdx(); ++i)
  {
    subroutes.emplace_back(
        geometry::PointWithAltitude(checkpoints.GetPoint(i), geometry::kDefaultAltitudeMeters),
        geometry::PointWithAltitude(checkpoints.GetPoint(i + 1), geometry::kDefaultAltitudeMeters),
        0 /* beginSegmentIdx */, 0 /* endSegmentIdx */);
  }
}

bool GetLastRealOrPart(IndexGraphStarter const & starter, vector<Segment> const & route,
                       Segment & real)
{
  for (auto rit = route.rbegin(); rit != route.rend(); ++rit)
  {
    real = *rit;
    if (starter.ConvertToReal(real))
      return true;
  }
  return false;
}

// Returns true if seg1 and seg2 have the same geometry.
bool IsTheSameSegments(m2::PointD const & seg1From, m2::PointD const & seg1To,
                       m2::PointD const & seg2From, m2::PointD const & seg2To)
{
  return (seg1From == seg2From && seg1To == seg2To) || (seg1From == seg2To && seg1To == seg2From);
}

bool IsDeadEnd(Segment const & segment, bool isOutgoing, bool useRoutingOptions,
               WorldGraph & worldGraph, set<Segment> & visitedSegments)
{
  // Maximum size (in Segment) of an island in road graph which may be found by the method.
  size_t constexpr kDeadEndTestLimit = 250;

  return !CheckGraphConnectivity(segment, isOutgoing, useRoutingOptions, kDeadEndTestLimit,
                                 worldGraph, visitedSegments);
}

bool IsDeadEndCached(Segment const & segment, bool isOutgoing, bool useRoutingOptions,
                     WorldGraph & worldGraph, set<Segment> & deadEnds)
{
  if (deadEnds.count(segment) != 0)
    return true;

  set<Segment> visitedSegments;
  if (IsDeadEnd(segment, isOutgoing, useRoutingOptions, worldGraph, visitedSegments))
  {
    deadEnds.insert(visitedSegments.begin(), visitedSegments.end());
    return true;
  }

  return false;
}
}  // namespace


// IndexRouter::BestEdgeComparator ----------------------------------------------------------------
IndexRouter::BestEdgeComparator::BestEdgeComparator(m2::PointD const & point, m2::PointD const & direction)
  : m_point(point), m_direction(direction)
{
}

int IndexRouter::BestEdgeComparator::Compare(Edge const & edge1, Edge const & edge2) const
{
  if (IsDirectionValid())
  {
    bool const isEdge1Codirectional = IsAlmostCodirectional(edge1);
    if (isEdge1Codirectional != IsAlmostCodirectional(edge2))
      return isEdge1Codirectional ? -1 : 1;
  }

  double const squaredDistFromEdge1 = GetSquaredDist(edge1);
  double const squaredDistFromEdge2 = GetSquaredDist(edge2);
  if (squaredDistFromEdge1 == squaredDistFromEdge2)
    return 0;

  return squaredDistFromEdge1 < squaredDistFromEdge2 ? -1 : 1;
}

bool IndexRouter::BestEdgeComparator::IsAlmostCodirectional(Edge const & edge) const
{
  CHECK(IsDirectionValid(), ());

  auto const edgeDirection = edge.GetDirection();
  if (edgeDirection.IsAlmostZero())
    return false;

  double const cosAng = m2::DotProduct(m_direction.Normalize(), edgeDirection.Normalize());
  // Note. acos(0.97) ≈ 14 degrees.
  double constexpr kMinCosAngForAlmostCodirectionalVectors = 0.97;
  return cosAng >= kMinCosAngForAlmostCodirectionalVectors;
}

double IndexRouter::BestEdgeComparator::GetSquaredDist(Edge const & edge) const
{
  m2::ParametrizedSegment<m2::PointD> const segment(edge.GetStartJunction().GetPoint(),
                                                    edge.GetEndJunction().GetPoint());
  return segment.SquaredDistanceToPoint(m_point);
}

// IndexRouter ------------------------------------------------------------------------------------
IndexRouter::IndexRouter(VehicleType vehicleType, bool loadAltitudes,
                         CountryParentNameGetterFn const & countryParentNameGetterFn,
                         TCountryFileFn const & countryFileFn, CountryRectFn const & countryRectFn,
                         shared_ptr<NumMwmIds> numMwmIds, unique_ptr<m4::Tree<NumMwmId>> numMwmTree,
                         traffic::TrafficCache const & trafficCache, DataSource & dataSource)
  : m_vehicleType(vehicleType)
  , m_loadAltitudes(loadAltitudes)
  , m_name("astar-bidirectional-" + ToString(m_vehicleType))
  , m_dataSource(dataSource, numMwmIds)
  , m_vehicleModelFactory(CreateVehicleModelFactory(m_vehicleType, countryParentNameGetterFn))
  , m_countryFileFn(countryFileFn)
  , m_countryRectFn(countryRectFn)
  , m_numMwmIds(std::move(numMwmIds))
  , m_numMwmTree(std::move(numMwmTree))
  , m_trafficStash(CreateTrafficStash(m_vehicleType, m_numMwmIds, trafficCache))
  , m_roadGraph(m_dataSource,
                vehicleType == VehicleType::Pedestrian || vehicleType == VehicleType::Transit
                    ? IRoadGraph::Mode::IgnoreOnewayTag
                    : IRoadGraph::Mode::ObeyOnewayTag,
                m_vehicleModelFactory)
  , m_estimator(EdgeEstimator::Create(
        m_vehicleType, CalcMaxSpeed(*m_numMwmIds, *m_vehicleModelFactory, m_vehicleType),
        CalcOffroadSpeed(*m_vehicleModelFactory), m_trafficStash,
        &dataSource, m_numMwmIds))
  , m_directionsEngine(CreateDirectionsEngine(m_vehicleType, m_numMwmIds, m_dataSource))
  , m_countryParentNameGetterFn(countryParentNameGetterFn)
{
  CHECK(!m_name.empty(), ());
  CHECK(m_numMwmIds, ());
  CHECK(m_numMwmTree, ());
  CHECK(m_estimator, ());
  CHECK(m_directionsEngine, ());
}

unique_ptr<WorldGraph> IndexRouter::MakeSingleMwmWorldGraph()
{
  auto worldGraph = MakeWorldGraph();
  worldGraph->SetMode(WorldGraphMode::SingleMwm);
  return worldGraph;
}

void IndexRouter::ClearState()
{
  m_roadGraph.ClearState();
  m_directionsEngine->Clear();
  m_dataSource.FreeHandles();
}

bool IndexRouter::FindClosestProjectionToRoad(m2::PointD const & point,
                                              m2::PointD const & direction, double radius,
                                              EdgeProj & proj)
{
  auto const rect = mercator::RectByCenterXYAndSizeInMeters(point, radius);
  std::vector<EdgeProjectionT> candidates;

  uint32_t const count = direction.IsAlmostZero() ? 1 : 4;
  m_roadGraph.FindClosestEdges(rect, count, candidates);

  if (candidates.empty())
    return false;

  if (direction.IsAlmostZero())
  {
    // We have no direction so return the first closest edge.
    proj.m_edge = candidates[0].first;
    proj.m_point = candidates[0].second.GetPoint();
    return true;
  }

  // We have direction so we can find the closest codirectional edge.
  Edge codirectionalEdge;
  if (!PointsOnEdgesSnapping::FindClosestCodirectionalEdge(point, direction, candidates, codirectionalEdge))
    return false;

  for (auto const & [edge, projection] : candidates)
  {
    if (edge == codirectionalEdge)
    {
      proj.m_edge = edge;
      proj.m_point = projection.GetPoint();
      break;
    }
  }

  return true;
}

void IndexRouter::SetGuides(GuidesTracks && guides) { m_guides = GuidesConnections(guides); }

RouterResultCode IndexRouter::CalculateRoute(Checkpoints const & checkpoints,
                                             m2::PointD const & startDirection,
                                             bool adjustToPrevRoute,
                                             RouterDelegate const & delegate, Route & route)
{
  auto const & startPoint = checkpoints.GetStart();
  auto const & finalPoint = checkpoints.GetFinish();

  try
  {
    SCOPE_GUARD(featureRoadGraphClear, [this]
    {
      ClearState();
    });

    if (adjustToPrevRoute && m_lastRoute && m_lastFakeEdges &&
        finalPoint == m_lastRoute->GetFinish())
    {
      double const distanceToRoute = m_lastRoute->CalcDistance(startPoint);
      double const distanceToFinish = mercator::DistanceOnEarth(startPoint, finalPoint);
      if (distanceToRoute <= kAdjustRangeM && distanceToFinish >= kMinDistanceToFinishM)
      {
        auto const code = AdjustRoute(checkpoints, startDirection, delegate, route);
        if (code != RouterResultCode::RouteNotFound)
          return code;

        LOG(LWARNING, ("Can't adjust route, do full rebuild, prev start:", mercator::ToLatLon(m_lastRoute->GetStart()),
                       "start:", mercator::ToLatLon(startPoint), "finish:", mercator::ToLatLon(finalPoint)));
      }
    }

    return DoCalculateRoute(checkpoints, startDirection, delegate, route);
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("Can't find path from", mercator::ToLatLon(startPoint), "to",
                 mercator::ToLatLon(finalPoint), ":\n ", e.what()));
    return RouterResultCode::InternalError;
  }
}

std::vector<Segment> IndexRouter::GetBestOutgoingSegments(m2::PointD const & checkpoint, WorldGraph & graph)
{
  bool dummy = false;
  std::vector<Segment> segments;

  PointsOnEdgesSnapping snapping(*this, graph);
  snapping.FindBestSegments(checkpoint, m2::PointD::Zero() /* startDirection */, true /* isOutgoing */, segments, dummy);

  return segments;
}

bool IndexRouter::GetBestOutgoingEdges(m2::PointD const & checkpoint, WorldGraph & graph, std::vector<Edge> & edges)
{
  bool dummy = false;
  PointsOnEdgesSnapping snapping(*this, graph);
  return snapping.FindBestEdges(checkpoint, m2::PointD::Zero() /* startDirection */, true /* isOutgoing */,
                                FeaturesRoadGraph::kClosestEdgesRadiusM, edges, dummy);
}

void IndexRouter::AppendPartsOfReal(LatLonWithAltitude const & point1,
                                    LatLonWithAltitude const & point2, uint32_t & startIdx,
                                    ConnectionToOsm & link)
{
  uint32_t const fakeFeatureId = FakeFeatureIds::kIndexGraphStarterId;

  FakeVertex vertexForward(m_guides.GetMwmId(), point1 /* from */, point2 /* to */,
                           FakeVertex::Type::PartOfReal);

  FakeVertex vertexBackward(m_guides.GetMwmId(), point2 /* from */, point1 /* to */,
                            FakeVertex::Type::PartOfReal);

  link.m_partsOfReal.emplace_back(
      vertexForward, Segment(kFakeNumMwmId, fakeFeatureId, startIdx++, true /* forward */));
  link.m_partsOfReal.emplace_back(
      vertexBackward, Segment(kFakeNumMwmId, fakeFeatureId, startIdx++, true /* forward */));
}

void IndexRouter::ConnectCheckpointsOnGuidesToOsm(std::vector<m2::PointD> const & checkpoints,
                                                  WorldGraph & graph)
{
  for (size_t checkpointIdx = 0; checkpointIdx < checkpoints.size(); ++checkpointIdx)
  {
    if (!m_guides.IsCheckpointAttached(checkpointIdx))
      continue;

    if (m_guides.FitsForDirectLinkToGuide(checkpointIdx, checkpoints.size()))
      continue;

    // Projection of this checkpoint is not connected to OSM.
    if (m_guides.GetOsmConnections(checkpointIdx).empty())
      continue;

    auto const & checkpoint = checkpoints[checkpointIdx];
    auto const & bestSegmentsOsm = GetBestOutgoingSegments(checkpoint, graph);
    if (bestSegmentsOsm.empty())
      continue;

    m_guides.OverwriteFakeEnding(checkpointIdx, MakeFakeEnding(bestSegmentsOsm, checkpoint, graph));
  }
}

uint32_t IndexRouter::ConnectTracksOnGuidesToOsm(std::vector<m2::PointD> const & checkpoints,
                                                 WorldGraph & graph)
{
  uint32_t segmentIdx = 0;

  for (size_t checkpointIdx = 0; checkpointIdx < checkpoints.size(); ++checkpointIdx)
  {
    auto osmConnections = m_guides.GetOsmConnections(checkpointIdx);

    if (osmConnections.empty())
      continue;

    bool foundSegmentsForProjection = false;
    for (size_t i = 0; i < osmConnections.size(); ++i)
    {
      auto & link = osmConnections[i];
      geometry::PointWithAltitude const & proj = link.m_projectedPoint;

      auto const & segmentsProj = GetBestOutgoingSegments(proj.GetPoint(), graph);
      if (segmentsProj.empty())
        continue;

      if (link.m_fromCheckpoint)
        foundSegmentsForProjection = true;

      auto newFakeEndingProj = MakeFakeEnding(segmentsProj, proj.GetPoint(), graph);

      if (link.m_fakeEnding.m_projections.empty())
        link.m_fakeEnding = newFakeEndingProj;
      else
        GuidesConnections::ExtendFakeEndingProjections(newFakeEndingProj, link.m_fakeEnding);

      LatLonWithAltitude const loopPoint = link.m_loopVertex.GetJunctionTo();

      if (!(loopPoint == link.m_realFrom))
        AppendPartsOfReal(link.m_realFrom, loopPoint, segmentIdx, link);

      if (!(loopPoint == link.m_realTo))
        AppendPartsOfReal(loopPoint, link.m_realTo, segmentIdx, link);
    }
    if (foundSegmentsForProjection)
      m_guides.UpdateOsmConnections(checkpointIdx, osmConnections);
    else
      m_guides.UpdateOsmConnections(checkpointIdx, {});
  }
  return segmentIdx;
}

void IndexRouter::AddGuidesOsmConnectionsToGraphStarter(size_t checkpointIdxFrom,
                                                        size_t checkpointIdxTo,
                                                        IndexGraphStarter & starter)
{
  auto linksFrom = m_guides.GetOsmConnections(checkpointIdxFrom);
  auto linksTo = m_guides.GetOsmConnections(checkpointIdxTo);
  for (auto const & link : linksTo)
  {
    auto it = find_if(linksFrom.begin(), linksFrom.end(), [&link](ConnectionToOsm const & cur) {
      return link.m_fakeEnding.m_originJunction == cur.m_fakeEnding.m_originJunction &&
             link.m_fakeEnding.m_projections == cur.m_fakeEnding.m_projections &&
             link.m_realSegment == cur.m_realSegment && link.m_realTo == cur.m_realTo;
    });

    if (it == linksFrom.end())
    {
      linksFrom.push_back(link);
    }
  }

  for (auto & connOsm : linksFrom)
  {
    if (connOsm.m_fakeEnding.m_projections.empty())
      continue;

    starter.AddEnding(connOsm.m_fakeEnding);

    starter.ConnectLoopToGuideSegments(connOsm.m_loopVertex, connOsm.m_realSegment,
                                       connOsm.m_realFrom, connOsm.m_realTo, connOsm.m_partsOfReal);
  }
}

RouterResultCode IndexRouter::DoCalculateRoute(Checkpoints const & checkpoints,
                                               m2::PointD const & startDirection,
                                               RouterDelegate const & delegate, Route & route)
{
  m_lastRoute.reset();
  // MwmId used for guides segments in RedressRoute().
  NumMwmId guidesMwmId = kFakeNumMwmId;

  for (auto const & checkpoint : checkpoints.GetPoints())
  {
    auto const country = platform::CountryFile(m_countryFileFn(checkpoint));

    if (country.IsEmpty())
    {
      /// @todo Can we pass an error into \a route instance with final message like:
      /// "Please, try to move start/end point a bit .."

      LOG(LWARNING, ("For point", mercator::ToLatLon(checkpoint),
                   "CountryInfoGetter returns an empty CountryFile(). It happens when checkpoint "
                   "is put at gaps between mwm."));
      return RouterResultCode::InternalError;
    }

    if (!m_dataSource.IsLoaded(country))
    {
      route.AddAbsentCountry(country.GetName());
    }
    else if (guidesMwmId == kFakeNumMwmId)
    {
      guidesMwmId = m_numMwmIds->GetId(country);
    }
  }

  if (!route.GetAbsentCountries().empty())
    return RouterResultCode::NeedMoreMaps;

  TrafficStash::Guard guard(m_trafficStash);
  unique_ptr<WorldGraph> graph = MakeWorldGraph();

  vector<Segment> segments;

  m_guides.SetGuidesGraphParams(guidesMwmId, m_estimator->GetMaxWeightSpeedMpS());
  m_guides.ConnectToGuidesGraph(checkpoints.GetPoints());

  uint32_t const startIdx = ConnectTracksOnGuidesToOsm(checkpoints.GetPoints(), *graph);
  ConnectCheckpointsOnGuidesToOsm(checkpoints.GetPoints(), *graph);

  size_t subrouteSegmentsBegin = 0;
  vector<Route::SubrouteAttrs> subroutes;
  PushPassedSubroutes(checkpoints, subroutes);

  unique_ptr<IndexGraphStarter> starter;

  auto progress = make_shared<AStarProgress>();
  double const checkpointsLength = checkpoints.GetSummaryLengthBetweenPointsMeters();

  PointsOnEdgesSnapping snapping(*this, *graph);
  size_t const subroutesCount = checkpoints.GetNumSubroutes();
  for (size_t i = checkpoints.GetPassedIdx(); i < subroutesCount; ++i)
  {
    auto const & startCheckpoint = checkpoints.GetPoint(i);
    auto const & finishCheckpoint = checkpoints.GetPoint(i + 1);

    FakeEnding startFakeEnding = m_guides.GetFakeEnding(i);
    FakeEnding finishFakeEnding = m_guides.GetFakeEnding(i + 1);

    bool isStartSegmentStrictForward = (m_vehicleType == VehicleType::Car);
    if (startFakeEnding.m_projections.empty() || finishFakeEnding.m_projections.empty())
    {
      bool const isFirstSubroute = (i == checkpoints.GetPassedIdx());
      bool const isLastSubroute = (i == subroutesCount - 1);

      bool startIsCodirectional = false;
      switch (snapping.Snap(startCheckpoint, finishCheckpoint, startDirection,
                            startFakeEnding, finishFakeEnding, startIsCodirectional))
      {
      case 1: return RouterResultCode::StartPointNotFound;
      case 2: return isLastSubroute ? RouterResultCode::EndPointNotFound : RouterResultCode::IntermediatePointNotFound;
      }

      if (isFirstSubroute)
        isStartSegmentStrictForward = startIsCodirectional;
    }

    uint32_t const fakeNumerationStart = starter ? starter->GetNumFakeSegments() + startIdx : startIdx;
    IndexGraphStarter subrouteStarter(startFakeEnding, finishFakeEnding, fakeNumerationStart,
                                      isStartSegmentStrictForward, *graph);

    if (m_guides.IsAttached())
    {
      subrouteStarter.SetGuides(m_guides.GetGuidesGraph());
      AddGuidesOsmConnectionsToGraphStarter(i, i + 1, subrouteStarter);
    }

    vector<Segment> subroute;
    double contributionCoef = kAlmostZeroContribution;
    if (!AlmostEqualAbs(checkpointsLength, 0.0, 1e-5))
    {
      contributionCoef =
          mercator::DistanceOnEarth(startCheckpoint, finishCheckpoint) / checkpointsLength;
    }

    AStarSubProgress subProgress(mercator::ToLatLon(startCheckpoint),
                                 mercator::ToLatLon(finishCheckpoint), contributionCoef);
    progress->AppendSubProgress(subProgress);
    SCOPE_GUARD(eraseProgress, [&progress]() { progress->PushAndDropLastSubProgress(); });

    auto const result = CalculateSubroute(checkpoints, i, delegate, progress, subrouteStarter,
                                          subroute, m_guides.IsAttached());

    if (result != RouterResultCode::NoError)
      return result;

    IndexGraphStarter::CheckValidRoute(subroute);

    segments.insert(segments.end(), subroute.begin(), subroute.end());

    size_t subrouteSegmentsEnd = segments.size();
    subroutes.emplace_back(subrouteStarter.GetStartJunction().ToPointWithAltitude(),
                           subrouteStarter.GetFinishJunction().ToPointWithAltitude(),
                           subrouteSegmentsBegin, subrouteSegmentsEnd);
    subrouteSegmentsBegin = subrouteSegmentsEnd;

    // For every subroute except for the first one the last real segment is used as a next start segment.
    // It's implemented this way to prevent jumping from one road to another one using a via point.
    Segment nextSegment;
    CHECK(GetLastRealOrPart(subrouteStarter, subroute, nextSegment),
          ("No real or part of real segments in route."));
    snapping.SetNextStartSegment(nextSegment);

    if (!starter)
      starter = make_unique<IndexGraphStarter>(std::move(subrouteStarter));
    else
      starter->Append(FakeEdgesContainer(std::move(subrouteStarter)));
  }

  route.SetCurrentSubrouteIdx(checkpoints.GetPassedIdx());
  route.SetSubroteAttrs(std::move(subroutes));

  IndexGraphStarter::CheckValidRoute(segments);

  // TODO (@gmoryes) https://jira.mail.ru/browse/MAPSME-10694
  //  We should do RedressRoute for each subroute separately.
  auto redressResult = RedressRoute(segments, delegate.GetCancellable(), *starter, route);
  if (redressResult != RouterResultCode::NoError)
    return redressResult;

  LOG(LINFO, ("Route length:", route.GetTotalDistanceMeters(), "meters. ETA:",
      route.GetTotalTimeSec(), "seconds."));

  m_lastRoute = make_unique<SegmentedRoute>(checkpoints.GetStart(), checkpoints.GetFinish(),
                                            route.GetSubroutes());
  for (Segment const & segment : segments)
    m_lastRoute->AddStep(segment, mercator::FromLatLon(starter->GetPoint(segment, true /* front */)));

  m_lastFakeEdges = make_unique<FakeEdgesContainer>(std::move(*starter));

  return RouterResultCode::NoError;
}

vector<Segment> ProcessJoints(vector<JointSegment> const & jointsPath,
                              IndexGraphStarterJoints<IndexGraphStarter> & jointStarter)
{
  CHECK(!jointsPath.empty(), ());

  vector<Segment> path;
  for (auto const & joint : jointsPath)
  {
    vector<Segment> jointPath = jointStarter.ReconstructJoint(joint);
    if (jointPath.empty())
      continue;

    if (path.empty())
    {
      path = std::move(jointPath);
      continue;
    }

    auto begIter = jointPath.begin();
    if (path.back() == jointPath.front())
      ++begIter;
    path.insert(path.end(), begIter, jointPath.end());
  }

  return path;
}

RouterResultCode IndexRouter::CalculateSubroute(Checkpoints const & checkpoints, size_t subrouteIdx,
                                                RouterDelegate const & delegate,
                                                shared_ptr<AStarProgress> const & progress,
                                                IndexGraphStarter & starter,
                                                vector<Segment> & subroute,
                                                bool guidesActive /* = false */)
{
  subroute.clear();

  SetupAlgorithmMode(starter, guidesActive);

  WorldGraphMode const mode = starter.GetGraph().GetMode();
  LOG(LINFO, ("Routing in mode:", mode));

  base::ScopedTimerWithLog timer("Route build");
  switch (mode)
  {
  case WorldGraphMode::Joints:
    return CalculateSubrouteJointsMode(starter, delegate, progress, subroute);
  case WorldGraphMode::NoLeaps:
    return CalculateSubrouteNoLeapsMode(starter, delegate, progress, subroute);
  case WorldGraphMode::LeapsOnly:
    return CalculateSubrouteLeapsOnlyMode(checkpoints, subrouteIdx, starter, delegate, progress,
                                          subroute);
  default: CHECK(false, ("Wrong WorldGraphMode here:", mode));
  }
  UNREACHABLE();
}

RouterResultCode IndexRouter::CalculateSubrouteJointsMode(
    IndexGraphStarter & starter, RouterDelegate const & delegate,
    shared_ptr<AStarProgress> const & progress, vector<Segment> & subroute)
{
  using JointsStarter = IndexGraphStarterJoints<IndexGraphStarter>;
  JointsStarter jointStarter(starter, starter.GetStartSegment(), starter.GetFinishSegment());

  using Visitor = JunctionVisitor<JointsStarter>;
  Visitor visitor(jointStarter, delegate, kVisitPeriod, progress);

  using Vertex = JointsStarter::Vertex;
  using Edge = JointsStarter::Edge;
  using Weight = JointsStarter::Weight;

  AStarAlgorithm<Vertex, Edge, Weight>::Params<Visitor, AStarLengthChecker> params(
      jointStarter, jointStarter.GetStartJoint(), jointStarter.GetFinishJoint(),
      delegate.GetCancellable(), std::move(visitor),
      AStarLengthChecker(starter));

  RoutingResult<Vertex, Weight> routingResult;
  RouterResultCode const result = FindPath<Vertex, Edge, Weight>(params, {} /* mwmIds */, routingResult);

  if (result != RouterResultCode::NoError)
    return result;

  LOG(LDEBUG, ("Result route weight:", routingResult.m_distance));
  subroute = ProcessJoints(routingResult.m_path, jointStarter);
  return result;
}

RouterResultCode IndexRouter::CalculateSubrouteNoLeapsMode(
    IndexGraphStarter & starter, RouterDelegate const & delegate,
    shared_ptr<AStarProgress> const & progress, vector<Segment> & subroute)
{
  using Vertex = IndexGraphStarter::Vertex;
  using Edge = IndexGraphStarter::Edge;
  using Weight = IndexGraphStarter::Weight;

  using Visitor = JunctionVisitor<IndexGraphStarter>;
  Visitor visitor(starter, delegate, kVisitPeriod, progress);

  AStarAlgorithm<Vertex, Edge, Weight>::Params<Visitor, AStarLengthChecker> params(
      starter, starter.GetStartSegment(), starter.GetFinishSegment(),
      delegate.GetCancellable(), std::move(visitor), AStarLengthChecker(starter));

  RoutingResult<Vertex, Weight> routingResult;
  set<NumMwmId> const mwmIds = starter.GetMwms();
  RouterResultCode const result = FindPath<Vertex, Edge, Weight>(params, mwmIds, routingResult);

  if (result != RouterResultCode::NoError)
    return result;

  LOG(LDEBUG, ("Result route weight:", routingResult.m_distance));
  subroute = std::move(routingResult.m_path);
  return result;
}

namespace
{
void CollapseForward_ReverseLoops(std::vector<Segment> & path)
{
  for (size_t i = 1; i < path.size() - 2;)
  {
    auto const beg = path.begin() + i;
    auto const end = path.end() - 1;
    auto j = std::find(beg + 1, end, path[i].GetReversed());
    if (j != end)
      path.erase(beg, j + 1);
    else
      ++i;
  }
}
}

RouterResultCode IndexRouter::CalculateSubrouteLeapsOnlyMode(
    Checkpoints const & checkpoints, size_t subrouteIdx, IndexGraphStarter & starter,
    RouterDelegate const & delegate, shared_ptr<AStarProgress> const & progress,
    vector<Segment> & subroute)
{
  using Vertex = LeapsGraph::Vertex;
  using Edge = LeapsGraph::Edge;
  using Weight = LeapsGraph::Weight;

  // Get cross-mwm routes-candidates.
  std::vector<RoutingResultT> candidates;
  std::vector<RouteWeight> candidateMidWeights;

  {
    LeapsGraph leapsGraph(starter, MwmHierarchyHandler(m_numMwmIds, m_countryParentNameGetterFn));

    AStarSubProgress leapsProgress(mercator::ToLatLon(checkpoints.GetPoint(subrouteIdx)),
                                   mercator::ToLatLon(checkpoints.GetPoint(subrouteIdx + 1)),
                                   kLeapsStageContribution);
    SCOPE_GUARD(progressGuard, [&progress]() { progress->PushAndDropLastSubProgress(); });
    progress->AppendSubProgress(leapsProgress);

    // No need to call CheckLength in cross-mwm routine, thus we avoid calling GetRoadGeometry().
    struct AlwaysTrue
    {
      bool operator() (Weight const &) const { return true; }
    };

    using Visitor = JunctionVisitor<LeapsGraph>;
    AStarAlgorithm<Vertex, Edge, Weight>::Params<Visitor, AlwaysTrue> params(
        leapsGraph, leapsGraph.GetStartSegment(), leapsGraph.GetFinishSegment(),
        delegate.GetCancellable(), Visitor(leapsGraph, delegate, kVisitPeriodForLeaps, progress),
        AlwaysTrue());

    params.m_badReducedWeight = [](Weight const &, Weight const &)
    {
      /// @see CrossMwmConnector::GetTransition comment.
      /// Unfortunately, reduced weight invariant in LeapsOnly mode doesn't work with the workaround above.
      return false;
    };

    // Use Feature's index as a key to avoid multiple vertices with the same feature but a bit different segment.
    using EdgeKeyT = std::array<uint32_t, 2>;
    vector<RoutingResultT> routes;
    set<EdgeKeyT> edges;
    set<uint32_t> keys[2];    // 0 - end vertex of the first edge; 1 - beg vertex of the last edge

    auto const getBegEnd = [](RoutingResultT const & r) -> EdgeKeyT
    {
      size_t const pathSize = r.m_path.size();
      ASSERT_GREATER(pathSize, 2, ());
      return { r.m_path[1].GetFeatureId(), r.m_path[pathSize - 2].GetFeatureId() };
    };

    /// @todo Looks like there is no big deal in this constant due to the bidirectional search.
    /// But still Zalau -> Tiburg lasts 2 minutes, so set some reasonable timeout.
    size_t constexpr kMaxVertices = 15;
    uint64_t constexpr kTimeoutMilliS = 30*1000;
    base::Timer timer;

    using AlgoT = AStarAlgorithm<Vertex, Edge, Weight>;
    auto const result = AlgoT().FindPathBidirectionalEx(params, [&](RoutingResultT && route)
    {
      // Take unique routes by key vertices.
      auto const be = getBegEnd(route);
      if (edges.insert(be).second)
      {
        keys[0].insert(be[0]);
        keys[1].insert(be[1]);
        routes.push_back(std::move(route));
      }

      // Stop if got needed number of vertices.
      if (keys[0].size() >= kMaxVertices && keys[1].size() >= kMaxVertices)
        return true;

      // Stop if have some routes and reached timeout.
      return (!routes.empty() && timer.ElapsedMilliseconds() > kTimeoutMilliS);
    });

    if (routes.empty() || result == AlgoT::Result::Cancelled)
      return ConvertResult<Vertex, Edge, Weight>(result);

    sort(routes.begin(), routes.end(), [](RoutingResultT const & l, RoutingResultT const & r)
    {
      return l.m_distance < r.m_distance;
    });

    // Additional heuristic to reduce number of candidates.
    // Group candidates by category and accept no more than *2* routes of each.
    // https://github.com/organicmaps/organicmaps/issues/2998
    // https://github.com/organicmaps/organicmaps/issues/3625
    class HighwayCategoryChecker
    {
      IndexGraphStarter const & m_starter;
      std::array<uint8_t, static_cast<uint8_t>(HighwayCategory::Unknown)> m_curr, m_upper;

    public:
      HighwayCategoryChecker(IndexGraphStarter const & starter) : m_starter(starter)
      {
        // Check Germany_Italy_Malcesine test when changing this constants.
        // 2 - max number of leaps candidates per highway category.
        // Total number of candidates will be definitely less than 2 * size(HighwayCategory).
        m_curr.fill(0);
        m_upper.fill(2);
      }
      bool operator()(Segment const & seg)
      {
        HighwayCategory const cat = m_starter.GetHighwayCategory(seg);
        if (cat == HighwayCategory::Unknown)
          return false;

        uint8_t const idx = static_cast<uint8_t>(cat);
        ASSERT_LESS(idx, m_curr.size(), ());
        return ++m_curr[idx] <= m_upper[idx];
      }
    };

    keys[0].clear();
    keys[1].clear();
    HighwayCategoryChecker checkers[2] = {starter, starter};

    for (auto & r : routes)
    {
      Segment const * be[] = { &r.m_path[1], &r.m_path[r.m_path.size() - 2] };
      for (uint8_t idx  = 0; idx < 2; ++idx)
      {
        if (keys[idx].insert(be[idx]->GetFeatureId()).second &&
            checkers[idx](*be[idx]))
        {
          uint8_t const nextIdx = (idx + 1) % 2;
          keys[nextIdx].insert(be[nextIdx]->GetFeatureId());
          checkers[nextIdx](*be[nextIdx]);

          candidates.push_back(std::move(r));
          break;
        }
      }
    }

    LOG(LINFO, ("Filtered candidates count =", candidates.size()));
    candidateMidWeights.reserve(candidates.size());
    for (auto & c : candidates)
    {
      CollapseForward_ReverseLoops(c.m_path);
      candidateMidWeights.push_back(leapsGraph.CalcMiddleCrossMwmWeight(c.m_path));
    }
  }

  // Purge cross-mwm-graph cache memory before calculating subroutes for each MWM.
  // CrossMwmConnector takes a lot of memory with its weights matrix now.
  starter.GetGraph().GetCrossMwmGraph().Purge();

  RoutesCalculator calculator(starter, delegate);
  RoutingResultT const * bestC = nullptr;

  {
    SCOPE_GUARD(progressGuard, [&progress]() { progress->PushAndDropLastSubProgress(); });
    progress->AppendSubProgress(AStarSubProgress(kCandidatesStageContribution));
    double const candidateContribution = kCandidatesStageContribution / (2 * candidates.size());

    // Select best candidate by calculating start/end sub-routes and using candidateMidWeights.
    RouteWeight bestW = GetAStarWeightMax<RouteWeight>();
    for (size_t i = 0; i < candidates.size(); ++i)
    {
      auto const & c = candidates[i];
      LOG(LDEBUG, ("Process leaps:", c.m_distance, c.m_path));

      size_t const sz = c.m_path.size();
      auto const * r1 = calculator.Calc2Times(c.m_path[0], c.m_path[1], progress, candidateContribution);
      auto const * r2 = calculator.Calc2Times(c.m_path[sz-2], c.m_path[sz-1], progress, candidateContribution);

      if (r1 && r2)
      {
        RouteWeight const w = r1->m_distance + candidateMidWeights[i] + r2->m_distance;
        if (w < bestW)
        {
          bestW = w;
          bestC = &c;
        }
      }
    }
  }

  SCOPE_GUARD(progressGuard, [&progress]() { progress->PushAndDropLastSubProgress(); });
  progress->AppendSubProgress(AStarSubProgress(1 - kLeapsStageContribution - kCandidatesStageContribution));

  if (bestC == nullptr)
    return RouterResultCode::RouteNotFound;

  // Calculate route for the best candidate.
  RoutingResultT result;
  ProcessLeapsJoints(bestC->m_path, starter, progress, calculator, result);

  if (result.Empty())
    return RouterResultCode::RouteNotFound;

  LOG(LDEBUG, ("Result route weight:", result.m_distance));

  /// @todo This routine is buggy and should be revised, I'm not sure if it's still needed ...
  /// Russia_UseDonMotorway test coordinates makes valid route until this point, but LeapsPostProcessor
  /// adds highway=service detour in this coordinates (53.8782154, 38.0483006) instead of keeping "Дон" motorway.
//  LeapsPostProcessor leapsPostProcessor(result.m_path, starter);
//  subroute = leapsPostProcessor.GetProcessedPath();
  subroute = std::move(result.m_path);

  return RouterResultCode::NoError;
}

RouterResultCode IndexRouter::AdjustRoute(Checkpoints const & checkpoints,
                                          m2::PointD const & startDirection,
                                          RouterDelegate const & delegate, Route & route)
{
  base::Timer timer;
  TrafficStash::Guard guard(m_trafficStash);
  auto graph = MakeWorldGraph();
  graph->SetMode(WorldGraphMode::NoLeaps);

  vector<Segment> startSegments;
  m2::PointD const & pointFrom = checkpoints.GetPointFrom();
  bool bestSegmentIsAlmostCodirectional = false;
  PointsOnEdgesSnapping snapping(*this, *graph);
  if (!snapping.FindBestSegments(pointFrom, startDirection, true /* isOutgoing */, startSegments,
                        bestSegmentIsAlmostCodirectional))
  {
    return RouterResultCode::StartPointNotFound;
  }

  auto const & lastSubroutes = m_lastRoute->GetSubroutes();
  CHECK(!lastSubroutes.empty(), ());
  auto const & lastSubroute = m_lastRoute->GetSubroute(checkpoints.GetPassedIdx());

  auto const & steps = m_lastRoute->GetSteps();
  CHECK(!steps.empty(), ());

  FakeEnding dummy{};
  IndexGraphStarter starter(MakeFakeEnding(startSegments, pointFrom, *graph), dummy,
                            m_lastFakeEdges->GetNumFakeEdges(), bestSegmentIsAlmostCodirectional,
                            *graph);

  starter.Append(*m_lastFakeEdges);

  vector<SegmentEdge> prevEdges;
  CHECK_LESS_OR_EQUAL(lastSubroute.GetEndSegmentIdx(), steps.size(), ());
  for (size_t i = lastSubroute.GetBeginSegmentIdx(); i < lastSubroute.GetEndSegmentIdx(); ++i)
  {
    auto const & step = steps[i];
    prevEdges.emplace_back(step.GetSegment(), starter.CalcSegmentWeight(step.GetSegment(),
                           EdgeEstimator::Purpose::Weight));
  }

  using Visitor = JunctionVisitor<IndexGraphStarter>;
  Visitor visitor(starter, delegate, kVisitPeriod);

  using Vertex = IndexGraphStarter::Vertex;
  using Edge = IndexGraphStarter::Edge;
  using Weight = IndexGraphStarter::Weight;

  AStarAlgorithm<Vertex, Edge, Weight> algorithm;
  AStarAlgorithm<Vertex, Edge, Weight>::Params<Visitor, AdjustLengthChecker> params(
      starter, starter.GetStartSegment(), {} /* finalVertex */,
      delegate.GetCancellable(), std::move(visitor), AdjustLengthChecker(starter));

  RoutingResult<Segment, RouteWeight> result;
  auto const resultCode =
      ConvertResult<Vertex, Edge, Weight>(algorithm.AdjustRoute(params, prevEdges, result));
  if (resultCode != RouterResultCode::NoError)
    return resultCode;

  CHECK_GREATER_OR_EQUAL(result.m_path.size(), 2, ());
  CHECK(IndexGraphStarter::IsFakeSegment(result.m_path.front()), ());
  CHECK(IndexGraphStarter::IsFakeSegment(result.m_path.back()), ());

  vector<Route::SubrouteAttrs> subroutes;
  PushPassedSubroutes(checkpoints, subroutes);

  size_t subrouteOffset = result.m_path.size();
  subroutes.emplace_back(starter.GetStartJunction().ToPointWithAltitude(),
                         starter.GetFinishJunction().ToPointWithAltitude(), 0 /* beginSegmentIdx */,
                         subrouteOffset);

  for (size_t i = checkpoints.GetPassedIdx() + 1; i < lastSubroutes.size(); ++i)
  {
    auto const & subroute = lastSubroutes[i];

    for (size_t j = subroute.GetBeginSegmentIdx(); j < subroute.GetEndSegmentIdx(); ++j)
      result.m_path.push_back(steps[j].GetSegment());

    subroutes.emplace_back(subroute, subrouteOffset);
    subrouteOffset = subroutes.back().GetEndSegmentIdx();
  }

  CHECK_EQUAL(result.m_path.size(), subrouteOffset, ());

  route.SetCurrentSubrouteIdx(checkpoints.GetPassedIdx());
  route.SetSubroteAttrs(std::move(subroutes));

  auto const redressResult = RedressRoute(result.m_path, delegate.GetCancellable(), starter, route);
  if (redressResult != RouterResultCode::NoError)
    return redressResult;

  LOG(LINFO, ("Adjust route, elapsed:", timer.ElapsedSeconds(), ", prev start:", checkpoints,
              ", prev route:", steps.size(), ", new route:", result.m_path.size()));

  return RouterResultCode::NoError;
}

unique_ptr<WorldGraph> IndexRouter::MakeWorldGraph()
{
  // Use saved routing options for all types (car, bicycle, pedestrian).
  RoutingOptions const routingOptions = RoutingOptions::LoadCarOptionsFromSettings();
  /// @DebugNote
  // Add avoid roads here for debug purpose.
  //routingOptions.Add(RoutingOptions::Road::Motorway);
  LOG(LINFO, ("Avoid next roads:", routingOptions));

  auto crossMwmGraph = make_unique<CrossMwmGraph>(
      m_numMwmIds, m_numMwmTree,
      m_vehicleType == VehicleType::Transit ? VehicleType::Pedestrian : m_vehicleType,
      m_countryRectFn, m_dataSource);

  auto indexGraphLoader = IndexGraphLoader::Create(
      m_vehicleType == VehicleType::Transit ? VehicleType::Pedestrian : m_vehicleType,
      m_loadAltitudes, m_vehicleModelFactory, m_estimator, m_dataSource, routingOptions);

  if (m_vehicleType != VehicleType::Transit)
  {
    auto graph = make_unique<SingleVehicleWorldGraph>(
        std::move(crossMwmGraph), std::move(indexGraphLoader), m_estimator,
        MwmHierarchyHandler(m_numMwmIds, m_countryParentNameGetterFn));
    graph->SetRoutingOptions(routingOptions);
    return graph;
  }

  auto transitGraphLoader = TransitGraphLoader::Create(m_dataSource, m_estimator);
  return make_unique<TransitWorldGraph>(std::move(crossMwmGraph), std::move(indexGraphLoader),
                                        std::move(transitGraphLoader), m_estimator);
}

int IndexRouter::PointsOnEdgesSnapping::Snap(
        m2::PointD const & start, m2::PointD const & finish, m2::PointD const & direction,
        FakeEnding & startEnding, FakeEnding & finishEnding, bool & startIsCodirectional)
{
  if (m_startSegments.empty())  // A first call for the first starting point
  {
    FillDeadEndsCache(finish);

    if (!FindBestSegments(start, direction, true /* isOutgoing */, m_startSegments, startIsCodirectional))
      return 1;
  }

  vector<Segment> finishSegments;
  bool dummy;
  if (!FindBestSegments(finish, {} /* direction */, false /* isOutgoing */, finishSegments, dummy))
    return 2;

  // One of startEnding or finishEnding will be empty here.
  if (startEnding.m_projections.empty())
    startEnding = MakeFakeEnding(m_startSegments, start, m_graph);

  if (finishEnding.m_projections.empty())
    finishEnding = MakeFakeEnding(finishSegments, finish, m_graph);

  return 0;
}

void IndexRouter::PointsOnEdgesSnapping::FillDeadEndsCache(m2::PointD const & point)
{
  auto const rect = mercator::RectByCenterXYAndSizeInMeters(point, kFirstSearchDistanceM);
  auto closestRoads = m_router.m_roadGraph.FindRoads(rect, [this](FeatureID const & fid)
  {
    auto const & info = fid.m_mwmId.GetInfo();
    return m_router.m_numMwmIds->ContainsFile(info->GetLocalFile().GetCountryFile());
  });

  m_deadEnds[0].clear();
  EraseIfDeadEnd(point, closestRoads, m_deadEnds[0]);

  vector<EdgeProjectionT> candidates;
  m_deadEnds[1].clear();
  RoadsToNearestEdges(point, closestRoads, [this](EdgeProjectionT const & proj)
  {
    auto const segment = GetSegmentByEdge(proj.first);
    return !IsDeadEndCached(segment, true /* isOutgoing */, true /* useRoutingOptions */, m_graph, m_deadEnds[1]);
  }, candidates);
}

void IndexRouter::PointsOnEdgesSnapping::EraseIfDeadEnd(
        m2::PointD const & checkpoint, vector<RoadInfoT> & roads, std::set<Segment> & deadEnds) const
{
  // |deadEnds| cache is necessary to minimize number of calls a time consumption IsDeadEnd() method.
  base::EraseIf(roads, [&](RoadInfoT const & fullRoadInfo)
  {
    auto const & junctions = fullRoadInfo.m_roadInfo.m_junctions;
    CHECK_GREATER_OR_EQUAL(junctions.size(), 2, ());
    auto const squaredDistAndIndex = m2::CalcMinSquaredDistance(junctions.begin(), junctions.end(), checkpoint);

    // Note. Checking if an edge goes to a dead end is a time consumption process.
    // So the number of checked edges should be minimized as possible.
    // Below a heuristic is used. If the closest to |checkpoint| segment of a feature
    // in forward direction is a dead end all segments of the feature is considered as dead ends.
    auto const segment = GetSegmentByEdge(Edge::MakeReal(fullRoadInfo.m_featureId, true /* forward */,
                                                         squaredDistAndIndex.second, junctions[0], junctions[1]));
    return (IsDeadEndCached(segment, true /* isOutgoing */, false /* useRoutingOptions */, m_graph, deadEnds) &&
            m_deadEnds[0].count(segment) == 0);
  });
}

// static
bool IndexRouter::PointsOnEdgesSnapping::IsFencedOff(
        m2::PointD const & point, EdgeProjectionT const & edgeProjection, vector<RoadInfoT> const & fences)
{
  auto const & edge = edgeProjection.first;
  auto const & projPoint = edgeProjection.second.GetPoint();

  for (auto const & fence : fences)
  {
    auto const & featureGeom = fence.m_roadInfo.m_junctions;
    for (size_t i = 1; i < featureGeom.size(); ++i)
    {
      auto const & fencePointFrom = featureGeom[i - 1];
      auto const & fencePointTo = featureGeom[i];
      if (IsTheSameSegments(fencePointFrom.GetPoint(), fencePointTo.GetPoint(),
                            edge.GetStartPoint(), edge.GetEndPoint()))
      {
        continue;
      }

      // If two segment are connected with its ends it's also considered as an
      // intersection according to m2::SegmentsIntersect(). On the other hand
      // it's possible that |projPoint| is an end point of |edge| and |edge|
      // is connected with other edges. To prevent fencing off such edges with their
      // neighboring edges the condition !m2::IsPointOnSegment() is added.
      if (m2::SegmentsIntersect(point, projPoint, fencePointFrom.GetPoint(),
                                fencePointTo.GetPoint()) &&
                                !m2::IsPointOnSegment(projPoint, fencePointFrom.GetPoint(),
                                fencePointTo.GetPoint()))
      {
        return true;
      }
    }
  }
  return false;
}

// static
void IndexRouter::PointsOnEdgesSnapping::RoadsToNearestEdges(
        m2::PointD const & point, vector<RoadInfoT> const & roads,
        IsEdgeProjGood const & isGood, vector<EdgeProjectionT> & edgeProj)
{
  NearestEdgeFinder finder(point, isGood);
  for (auto const & road : roads)
    finder.AddInformationSource(road);

  finder.MakeResult(edgeProj, kMaxRoadCandidates);
}

Segment IndexRouter::PointsOnEdgesSnapping::GetSegmentByEdge(Edge const & edge) const
{
  auto const & featureId = edge.GetFeatureId();
  auto const & info = featureId.m_mwmId.GetInfo();
  CHECK(info, ());
  auto const numMwmId = m_router.m_numMwmIds->GetId(info->GetLocalFile().GetCountryFile());
  return Segment(numMwmId, edge.GetFeatureId().m_index, edge.GetSegId(), edge.IsForward());
}

// static
bool IndexRouter::PointsOnEdgesSnapping::FindClosestCodirectionalEdge(
        m2::PointD const & point, m2::PointD const & direction,
        vector<EdgeProjectionT> const & candidates, Edge & closestCodirectionalEdge)
{
  double constexpr kInvalidDist = numeric_limits<double>::max();
  double squareDistToClosestCodirectionalEdgeM = kInvalidDist;

  IndexRouter::BestEdgeComparator bestEdgeComparator(point, direction);
  if (!bestEdgeComparator.IsDirectionValid())
    return false;

  for (auto const & c : candidates)
  {
    auto const & edge = c.first;
    if (!bestEdgeComparator.IsAlmostCodirectional(edge))
      continue;

    double const distM = bestEdgeComparator.GetSquaredDist(edge);
    if (distM >= squareDistToClosestCodirectionalEdgeM)
      continue;

    closestCodirectionalEdge = edge;
    squareDistToClosestCodirectionalEdgeM = distM;
  }

  return squareDistToClosestCodirectionalEdgeM != kInvalidDist;
}

bool IndexRouter::PointsOnEdgesSnapping::FindBestSegments(
        m2::PointD const & checkpoint, m2::PointD const & direction, bool isOutgoing,
        vector<Segment> & bestSegments, bool & bestSegmentIsAlmostCodirectional)
{
  vector<Edge> bestEdges;
  if (!FindBestEdges(checkpoint, direction, isOutgoing, kFirstSearchDistanceM /* closestEdgesRadiusM */,
                     bestEdges, bestSegmentIsAlmostCodirectional))
  {
    if (!FindBestEdges(checkpoint, direction, isOutgoing, 500.0 /* closestEdgesRadiusM */,
                       bestEdges, bestSegmentIsAlmostCodirectional) &&
                       bestEdges.size() < kMaxRoadCandidates)
    {
      if (!FindBestEdges(checkpoint, direction, isOutgoing, 2000.0 /* closestEdgesRadiusM */,
                         bestEdges, bestSegmentIsAlmostCodirectional))
      {
        return false;
      }
    }
  }

  bestSegments.clear();
  for (auto const & edge : bestEdges)
    bestSegments.emplace_back(GetSegmentByEdge(edge));

  return true;
}

bool IndexRouter::PointsOnEdgesSnapping::FindBestEdges(
        m2::PointD const & checkpoint, m2::PointD const & direction, bool isOutgoing, double closestEdgesRadiusM,
        vector<Edge> & bestEdges, bool & bestSegmentIsAlmostCodirectional)
{
  auto const rect = mercator::RectByCenterXYAndSizeInMeters(checkpoint, closestEdgesRadiusM);
  auto closestRoads = m_router.m_roadGraph.FindRoads(rect, [this](FeatureID const & fid)
  {
    auto const & info = fid.m_mwmId.GetInfo();
    return m_router.m_numMwmIds->ContainsFile(info->GetLocalFile().GetCountryFile());
  });

  set<Segment> deadEnds[2];
  // Removing all dead ends from |closestRoads|. Then some candidates will be taken from |closestRoads|.
  // It's necessary to remove all dead ends for all |closestRoads| before IsFencedOff().
  // If to remove all fenced off by other features from |checkpoint| candidates at first,
  // only dead ends candidates may be left. And then the dead end candidates will be removed
  // as well as dead ends. It often happens near airports.
  EraseIfDeadEnd(checkpoint, closestRoads, deadEnds[0]);

  // Sorting from the closest features to the further ones. The idea is the closer
  // a feature to a |checkpoint| the more chances that it crosses the segment
  // |checkpoint|, projections of |checkpoint| on feature edges. It confirmed with benchmarks.
  sort(closestRoads.begin(), closestRoads.end(), [&checkpoint](RoadInfoT const & lhs, RoadInfoT const & rhs)
  {
    auto const & lj = lhs.m_roadInfo.m_junctions;
    auto const & rj = rhs.m_roadInfo.m_junctions;
    ASSERT(!lj.empty() && !rj.empty(), ());

    return checkpoint.SquaredLength(lj[0].GetPoint()) < checkpoint.SquaredLength(rj[0].GetPoint());
  });

  // Note about necessity of removing dead ends twice.
  // At first, only real dead ends and roads which are not correct according to |worldGraph|
  // are removed in EraseIfDeadEnd() function. It's necessary to prepare correct road network
  // (|closestRoads|) which will be used in IsFencedOff() method later and |closestRoads|
  // should contain all roads independently of routing options to prevent crossing roads
  // which are switched off in RoutingOptions.
  // Then in |IsDeadEndCached(..., true /* useRoutingOptions */, ...)| below we ignore
  // candidates if it's a dead end taking into acount routing options. We ignore candidates as well
  // if they don't match RoutingOptions.
  auto const isGood = [&](EdgeProjectionT const & edgeProj)
  {
    auto const segment = GetSegmentByEdge(edgeProj.first);
    if (IsDeadEndCached(segment, isOutgoing, true /* useRoutingOptions */, m_graph, deadEnds[1]) &&
        m_deadEnds[1].count(segment) == 0)
    {
      return false;
    }

    // Removing all candidates which are fenced off by the road graph (|closestRoads|) from |checkpoint|.
    return !IsFencedOff(checkpoint, edgeProj, closestRoads);
  };

  // Getting closest edges from |closestRoads| if they are correct according to isGood() function.
  vector<EdgeProjectionT> candidates;
  RoadsToNearestEdges(checkpoint, closestRoads, isGood, candidates);

  if (candidates.empty())
    return false;

  m_deadEnds[0].swap(deadEnds[0]);
  m_deadEnds[1].swap(deadEnds[1]);

  // Looking for the closest codirectional edge. If it's not found add all good candidates.
  Edge closestCodirectionalEdge;
  bestSegmentIsAlmostCodirectional =
      FindClosestCodirectionalEdge(checkpoint, direction, candidates, closestCodirectionalEdge);

  if (bestSegmentIsAlmostCodirectional)
  {
    bestEdges = {closestCodirectionalEdge};
  }
  else
  {
    bestEdges.clear();
    for (auto const & c : candidates)
      bestEdges.emplace_back(c.first);
  }

  return true;
}

IndexRouter::RoutingResultT const * IndexRouter::RoutesCalculator::Calc(
    Segment const & beg, Segment const & end, ProgressPtrT const & progress, double progressCoef)
{
  auto itCache = m_cache.insert({{beg, end}, {}});
  auto * res = &itCache.first->second;

  // Actually, we can (should?) append/push-drop progress even if the route is already in cache,
  // but I'd like to avoid this unnecessary actions here.
  if (itCache.second)
  {
    LOG(LDEBUG, ("Calculating sub-route:", beg, end));
    progress->AppendSubProgress({m_starter.GetPoint(beg, true), m_starter.GetPoint(end, true), progressCoef});

    using JointsStarter = IndexGraphStarterJoints<IndexGraphStarter>;
    JointsStarter jointStarter(m_starter);
    jointStarter.Init(beg, end);

    using Vertex = JointsStarter::Vertex;
    using Edge = JointsStarter::Edge;
    using Weight = JointsStarter::Weight;

    using Visitor = JunctionVisitor<JointsStarter>;
    AStarAlgorithm<Vertex, Edge, Weight>::Params<Visitor, AStarLengthChecker> params(
        jointStarter, jointStarter.GetStartJoint(), jointStarter.GetFinishJoint(),
        m_delegate.GetCancellable(), Visitor(jointStarter, m_delegate, kVisitPeriod, progress),
        AStarLengthChecker(m_starter));

    RoutingResult<JointSegment, RouteWeight> route;
    using AlgoT = AStarAlgorithm<Vertex, Edge, Weight>;

    if (AlgoT().FindPathBidirectional(params, route) == AlgoT::Result::OK)
    {
      LOG(LDEBUG, ("Sub-route weight:", route.m_distance));

      res->m_path = ProcessJoints(route.m_path, jointStarter);
      res->m_distance = route.m_distance;

      progress->PushAndDropLastSubProgress();
    }
    else
    {
      progress->DropLastSubProgress();

      m_cache.erase(itCache.first);
      return nullptr;
    }
  }

  CHECK(!res->Empty(), ());
  return res;
}

IndexRouter::RoutingResultT const * IndexRouter::RoutesCalculator::Calc2Times(
    Segment const & beg, Segment const & end, ProgressPtrT const & progress, double progressCoef)
{
  /// @see RussiaMoscow_ItalySienaCenter_SplittedMotorway
  /// Enter Tuscany motorway (43.5016115, 11.1872607) is splitted by MWM boundaries many times.

  m_starter.GetGraph().SetMode(WorldGraphMode::JointSingleMwm);
  auto const * r = Calc(beg, end, progress, progressCoef);
  if (r == nullptr)
  {
    m_starter.GetGraph().SetMode(WorldGraphMode::Joints);
    r = Calc(beg, end, progress, progressCoef);
  }
  return r;
}

RouterResultCode IndexRouter::ProcessLeapsJoints(vector<Segment> const & input,
                                                 IndexGraphStarter & starter,
                                                 shared_ptr<AStarProgress> const & progress,
                                                 RoutesCalculator & calculator,
                                                 RoutingResultT & result)
{
  LOG(LDEBUG, ("Process input:", input));

  // { fake, mwmId1, mwmId2, mwmId2, mwmId3, .. pairs of ids .. , fake }.
  ASSERT_GREATER_OR_EQUAL(input.size(), 4, ());
  ASSERT(input.size() % 2 == 0, ());

  WorldGraph & worldGraph = starter.GetGraph();

  // For all leaps except the first leap which connects start to mwm exit in LeapsOnly mode we need
  // to drop first segment of the leap because we already have its twin from the previous mwm.
  bool dropFirstSegment = false;

  // While route building in LeapOnly mode we estimate start to mwm exit distance. This estimation may
  // be incorrect and it causes appearance of unneeded loops: route goes from start to wrongly selected mwm exit,
  // then we have small unneeded leap in other mwm which returns us to start mwm, then we have leap in start mwm
  // to the correct start mwm exit and then we have normal route.
  // |input| mwm ids for such case look like
  // { fake, startId, otherId, otherId, startId, startId, .. pairs of ids for other leaps .. , finishId, fake }.

  // Stable solution here is to calculate all routes with and without loops, and choose the best one.
  // https://github.com/organicmaps/organicmaps/issues/821
  // https://github.com/organicmaps/organicmaps/issues/2085
  // https://github.com/organicmaps/organicmaps/issues/5069

  buffer_vector<size_t, 4> arrBeg, arrEnd;
  size_t const begIdx = 1;
  size_t const endIdx = input.size() - 2;

  auto const firstMwmId = input[begIdx].GetMwmId();
  for (size_t i = begIdx; i < endIdx; ++i)
    if (input[i].GetMwmId() == firstMwmId && (i % 2 == 1))
      arrBeg.push_back(i);

  auto const lastMwmId = input[endIdx].GetMwmId();
  for (size_t i = endIdx; i > begIdx; --i)
    if (input[i].GetMwmId() == lastMwmId && (i % 2 == 0))
      arrEnd.push_back(i);

  size_t const variantsCount = arrBeg.size() * arrEnd.size();
  ASSERT(variantsCount > 0, ());

  for (size_t startLeapEnd : arrBeg)
  for (size_t finishLeapStart : arrEnd)
  {
    size_t maxStart = 0;

    auto const runAStarAlgorithm = [&](size_t start, size_t end, WorldGraphMode mode)
    {
      ASSERT_LESS(start, input.size(), ());
      ASSERT_LESS(end, input.size(), ());

      maxStart = max(maxStart, start);
      auto const contribCoef = static_cast<double>(end - maxStart + 1) / input.size() / variantsCount;

      // VNG: I don't like this strategy with clearing previous caches, taking into account
      // that all MWMs were quite likely already loaded before in calculating Leaps path.
      // Clear previous loaded graphs to not spend too much memory at one time.
      //worldGraph.ClearCachedGraphs();
      worldGraph.SetMode(mode);

      return calculator.Calc(input[start], input[end], progress, contribCoef);
    };

    vector<vector<Segment>> paths;
    size_t prevStart = numeric_limits<size_t>::max();
    auto const tryBuildRoute = [&](size_t start, size_t end, WorldGraphMode mode)
    {
      auto const * res = runAStarAlgorithm(start, end, mode);
      if (res)
      {
        auto const & subroute = res->m_path;
        CHECK(!subroute.empty(), ());

        /// @todo How it's possible when prevStart == start ?
        if (start == prevStart && !paths.empty())
          paths.pop_back();

        auto begIter = subroute.cbegin();
        if (dropFirstSegment)
          ++begIter;
        paths.emplace_back(begIter, subroute.cend());

        dropFirstSegment = true;
        prevStart = start;
      }
      else
      {
        // This may happen when "Avoid roads option" is enabled.
        // Cross-mwm LeapsGraph returns paths regardless of this option.
        LOG(LINFO, ("Can not find path from:", starter.GetPoint(input[start], input[start].IsForward()),
                                        "to:", starter.GetPoint(input[end], input[end].IsForward())));
      }

      return res;
    };

    size_t lastPrev = 0;
    RouteWeight currentWeight = GetAStarWeightZero<RouteWeight>();
    RouteWeight lastWeight = currentWeight;

    for (size_t i = startLeapEnd; i <= finishLeapStart; ++i)
    {
      size_t prev, next;
      if (i == startLeapEnd)
      {
        prev = 0;
        next = i;
      }
      else if (i == finishLeapStart)
      {
        prev = i;
        next = input.size() - 1;
      }
      else
      {
        prev = i;
        next = i + 1;
      }

      RoutingResultT const * res = nullptr;
      if (res = tryBuildRoute(prev, next, WorldGraphMode::JointSingleMwm); !res)
      {
        // As written above, it may happen when "Avoid option" is enabled.
        // Possibile recover here is to shift |prev| and |next| to skip this "bad" MWM, and build a new
        // (mode = Joints) route (via several MWMs).
        // https://github.com/organicmaps/organicmaps/issues/5695

        /// @todo Anyway, this _recovering_ is a crutch. Should compare result.m_distance on caller side
        /// and call ProcessLeapsJoints for the next candidates (not for the "best" only).
        /// "Cross MWMs" together with "Avoid options" doesn't guarantee a best route, because
        /// cross-mwm graph keeps only _clean_ (no any avoids) weights.

        if (prev > 0)
        {
          prev = lastPrev;
          currentWeight = lastWeight;
          if (prev == 0)
            dropFirstSegment = false;
        }

        if (next + 2 > finishLeapStart)
          next = input.size() - 1;
        else
          next += 2;

        if (res = tryBuildRoute(prev, next, WorldGraphMode::Joints); !res)
          return RouterResultCode::RouteNotFound;
      }

      lastPrev = prev;
      lastWeight = currentWeight;
      i = next;

      currentWeight += res->m_distance;
    }

    // Update final result if new route is better.
    LOG(LDEBUG, ("Got a route:", startLeapEnd, finishLeapStart, currentWeight));
    if (result.Empty() || currentWeight < result.m_distance)
    {
      if (!result.Empty())
      {
        LOG(LDEBUG, ("Found better route, old weight =", result.m_distance, "new weight =", currentWeight));
        result.Clear();
      }

      for (auto const & e : paths)
        result.m_path.insert(result.m_path.end(), e.begin(), e.end());

      result.m_distance = currentWeight;
    }
  }

  return RouterResultCode::NoError;
}

RouterResultCode IndexRouter::RedressRoute(vector<Segment> const & segments,
                                           base::Cancellable const & cancellable,
                                           IndexGraphStarter & starter, Route & route)
{
  CHECK(!segments.empty(), ());
  IndexGraphStarter::CheckValidRoute(segments);

  size_t const segsCount = segments.size();
  vector<geometry::PointWithAltitude> junctions;
  junctions.reserve(segsCount + 1);

  for (size_t i = 0; i <= segsCount; ++i)
    junctions.emplace_back(starter.GetRouteJunction(segments, i).ToPointWithAltitude());

  IndexRoadGraph roadGraph(starter, segments, junctions, m_dataSource);
  starter.GetGraph().SetMode(WorldGraphMode::NoLeaps);

  vector<double> times;
  times.reserve(segments.size());

  // Time at first route point - weight of first segment.
  double time = starter.CalculateETAWithoutPenalty(segments.front());
  times.emplace_back(time);

  for (size_t i = 1; i < segments.size(); ++i)
  {
    time += starter.CalculateETA(segments[i - 1], segments[i]);
    times.emplace_back(time);
  }

  m_directionsEngine->SetVehicleType(m_vehicleType);
  ReconstructRoute(*m_directionsEngine, roadGraph, cancellable, junctions, times, route);

  if (cancellable.IsCancelled())
    return RouterResultCode::Cancelled;

  if (!route.IsValid())
  {
    LOG(LERROR, ("RedressRoute failed, segmenst count =", segments.size()));
    return RouterResultCode::RouteNotFoundRedressRouteError;
  }

  auto & worldGraph = starter.GetGraph();

  /// @todo I suspect that we can avoid calculating segments inside ReconstructRoute
  /// and use original |segments| (IndexRoadGraph::GetRouteSegments).
#ifdef DEBUG
  {
    auto const isPassThroughAllowed = [&worldGraph](Segment const & s)
    {
      return worldGraph.IsPassThroughAllowed(s.GetMwmId(), s.GetFeatureId());
    };

    auto const & rSegments = route.GetRouteSegments();
    ASSERT_EQUAL(segsCount, rSegments.size(), ());
    for (size_t i = 0; i < segsCount; ++i)
    {
      if (segments[i].IsRealSegment())
      {
        ASSERT_EQUAL(segments[i], rSegments[i].GetSegment(), ());

        if (i > 0 && segments[i - 1].IsRealSegment() &&
            isPassThroughAllowed(segments[i - 1]) != isPassThroughAllowed(segments[i]))
        {
          LOG(LDEBUG, ("Change pass-through point:", mercator::ToLatLon(rSegments[i - 1].GetJunction().GetPoint())));
        }
      }
    }
  }
#endif

  for (auto & routeSegment : route.GetRouteSegments())
  {
    auto & segment = routeSegment.GetSegment();
    routeSegment.SetTransitInfo(worldGraph.GetTransitInfo(segment));

    if (!m_guides.IsActive())
      routeSegment.SetRoadTypes(starter.GetRoutingOptions(segment));

    if (m_vehicleType == VehicleType::Car)
    {
      if (segment.IsRealSegment())
      {
        if (!AreSpeedCamerasProhibited(segment.GetMwmId()))
          routeSegment.SetSpeedCameraInfo(worldGraph.GetSpeedCamInfo(segment));

        if (m_trafficStash)
          routeSegment.SetTraffic(m_trafficStash->GetSpeedGroup(segment));

        routeSegment.SetSpeedLimit(worldGraph.GetSpeedLimit(segment));
      }
    }

    /// @todo By VNG: I suspect that we should convert the |segment| to a real one first, and fetch
    /// MaxSpeed, SpeedCamera, SpeedGroup, RoadTypes then, but current speed camera tests fail:
    /// speed_camera_notifications_tests.cpp
    if (!segment.IsRealSegment())
      starter.ConvertToReal(segment);
  }

  vector<platform::CountryFile> speedCamProhibited;
  FillSpeedCamProhibitedMwms(segments, speedCamProhibited);
  route.SetMwmsPartlyProhibitedForSpeedCams(std::move(speedCamProhibited));

  return RouterResultCode::NoError;
}

bool IndexRouter::AreSpeedCamerasProhibited(NumMwmId mwmID) const
{
  if (routing::AreSpeedCamerasProhibited(m_numMwmIds->GetFile(mwmID)))
  {
    // Not a big overhead here, but can cache flag in IndexRouter and reset it via
    // Framework -> RoutingSession -> IndexRouter.
    bool enabled = false;
    settings::TryGet(kDebugSpeedCamSetting, enabled);
    return !enabled;
  }
  return false;
}

bool IndexRouter::AreMwmsNear(IndexGraphStarter const & starter) const
{
  auto const & startMwmIds = starter.GetStartMwms();
  auto const & finishMwmIds = starter.GetFinishMwms();
  for (auto const startMwmId : startMwmIds)
  {
    m2::RectD const & rect = m_countryRectFn(m_numMwmIds->GetFile(startMwmId).GetName());
    bool found = false;
    m_numMwmTree->ForEachInRect(rect, [&finishMwmIds, &found](NumMwmId id)
    {
      if (!found && finishMwmIds.count(id) > 0)
        found = true;
    });
    if (found)
      return true;
  }

  return ms::DistanceOnEarth(starter.GetStartJunction().GetLatLon(),
                             starter.GetFinishJunction().GetLatLon()) < kCloseMwmPointsDistanceM;
}

bool IndexRouter::DoesTransitSectionExist(NumMwmId numMwmId)
{
  return m_dataSource.GetSectionStatus(numMwmId, TRANSIT_FILE_TAG) == MwmDataSource::SectionExists;
}

RouterResultCode IndexRouter::ConvertTransitResult(set<NumMwmId> const & mwmIds,
                                                   RouterResultCode resultCode)
{
  if (m_vehicleType != VehicleType::Transit || resultCode != RouterResultCode::RouteNotFound)
    return resultCode;

  for (auto const mwmId : mwmIds)
  {
    CHECK_NOT_EQUAL(mwmId, kFakeNumMwmId, ());
    if (!DoesTransitSectionExist(mwmId))
      return RouterResultCode::TransitRouteNotFoundNoNetwork;
  }

  return RouterResultCode::TransitRouteNotFoundTooLongPedestrian;
}

void IndexRouter::FillSpeedCamProhibitedMwms(vector<Segment> const & segments,
                                             vector<platform::CountryFile> & speedCamProhibitedMwms) const
{
  CHECK(m_numMwmIds, ());

  set<NumMwmId> mwmIds;
  for (auto const & s : segments)
    mwmIds.emplace(s.GetMwmId());

  for (auto const id : mwmIds)
  {
    if (id == kFakeNumMwmId)
      continue;

    auto const & country = m_numMwmIds->GetFile(id);
    if (AreSpeedCamerasPartlyProhibited(country))
      speedCamProhibitedMwms.emplace_back(country);
  }
}

void IndexRouter::SetupAlgorithmMode(IndexGraphStarter & starter, bool guidesActive) const
{
  // We use NoLeaps for pedestrians and bicycles with route points near to the Guides tracks
  // because it is much easier to implement. Otherwise for pedestrians and bicycles we use Joints.
  if (guidesActive)
  {
    starter.GetGraph().SetMode(WorldGraphMode::NoLeaps);
    return;
  }

  // We use leaps for cars only. Other vehicle types do not have weights in their cross-mwm sections.
  switch (m_vehicleType)
  {
  case VehicleType::Pedestrian:
  case VehicleType::Bicycle:
    starter.GetGraph().SetMode(WorldGraphMode::Joints);
    break;
  case VehicleType::Transit:
    starter.GetGraph().SetMode(WorldGraphMode::NoLeaps);
    break;
  case VehicleType::Car:
    starter.GetGraph().SetMode(AreMwmsNear(starter) ? WorldGraphMode::Joints
                                                    : WorldGraphMode::LeapsOnly);
    break;
  case VehicleType::Count:
    CHECK(false, ("Unknown vehicle type:", m_vehicleType));
    break;
  }
}
}  // namespace routing
