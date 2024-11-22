#include "routing/directions_engine.hpp"

#include "routing/data_source.hpp"
#include "routing/fake_feature_ids.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/turns.hpp"

#include "indexer/ftypes_matcher.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "geometry/point2d.hpp"

namespace routing
{
using namespace ftypes;
using namespace routing::turns;
using namespace std;

namespace
{
bool IsFakeFeature(uint32_t featureId)
{
  return routing::FakeFeatureIds::IsGuidesFeature(featureId) ||
         routing::FakeFeatureIds::IsTransitFeature(featureId);
}

feature::Metadata::EType GetLanesMetadataTag(FeatureType & ft, bool isForward)
{
  auto directionTag = isForward ? feature::Metadata::FMD_TURN_LANES_FORWARD
                                : feature::Metadata::FMD_TURN_LANES_BACKWARD;
  if (ft.HasMetadata(directionTag))
    return directionTag;
  return feature::Metadata::FMD_TURN_LANES;
}

void LoadLanes(LoadedPathSegment & pathSegment, FeatureType & ft, bool isForward)
{
  auto tag = GetLanesMetadataTag(ft, isForward);
  ParseLanes(std::string(ft.GetMetadata(tag)), pathSegment.m_lanes);
}
}  // namespace

DirectionsEngine::DirectionsEngine(MwmDataSource & dataSource, std::shared_ptr<NumMwmIds> numMwmIds)
  : m_dataSource(dataSource), m_numMwmIds(numMwmIds)
  , m_linkChecker(IsLinkChecker::Instance())
  , m_roundAboutChecker(IsRoundAboutChecker::Instance())
  , m_onewayChecker(IsOneWayChecker::Instance())
{
  CHECK(m_numMwmIds, ());
}

void DirectionsEngine::Clear()
{
  m_adjacentEdges.clear();
  m_pathSegments.clear();
}

unique_ptr<FeatureType> DirectionsEngine::GetFeature(FeatureID const & featureId)
{
  if (IsFakeFeature(featureId.m_index))
    return nullptr;
  return m_dataSource.GetFeature(featureId);
}

void DirectionsEngine::LoadPathAttributes(FeatureID const & featureId,
                                          LoadedPathSegment & pathSegment, bool isForward)
{
  if (!featureId.IsValid())
    return;

  auto ft = GetFeature(featureId);
  if (!ft)
    return;

  feature::TypesHolder types(*ft);

  LoadLanes(pathSegment, *ft, isForward);

  pathSegment.m_highwayClass = GetHighwayClass(types);
  ASSERT(pathSegment.m_highwayClass != HighwayClass::Undefined, (featureId));

  pathSegment.m_isLink = m_linkChecker(types);
  pathSegment.m_onRoundabout = m_roundAboutChecker(types);
  pathSegment.m_isOneWay = m_onewayChecker(types);

  pathSegment.m_roadNameInfo.m_isLink = pathSegment.m_isLink;
  pathSegment.m_roadNameInfo.m_junction_ref = ft->GetMetadata(feature::Metadata::FMD_JUNCTION_REF);
  pathSegment.m_roadNameInfo.m_destination_ref = ft->GetMetadata(feature::Metadata::FMD_DESTINATION_REF);
  pathSegment.m_roadNameInfo.m_destination = ft->GetMetadata(feature::Metadata::FMD_DESTINATION);
  /// @todo Should make some better parsing here (@see further use in GetFullRoadName).
  pathSegment.m_roadNameInfo.m_ref = ft->GetRef();
  pathSegment.m_roadNameInfo.m_name = ft->GetName(StringUtf8Multilang::kDefaultCode);
}

void DirectionsEngine::GetSegmentRangeAndAdjacentEdges(IRoadGraph::EdgeListT const & outgoingEdges,
                                                       Edge const & inEdge, uint32_t startSegId,
                                                       uint32_t endSegId,
                                                       SegmentRange & segmentRange,
                                                       TurnCandidates & outgoingTurns)
{
  outgoingTurns.isCandidatesAngleValid = true;
  outgoingTurns.candidates.reserve(outgoingEdges.size());
  segmentRange = SegmentRange(inEdge.GetFeatureId(), startSegId, endSegId, inEdge.IsForward(),
                              inEdge.GetStartPoint(), inEdge.GetEndPoint());
  CHECK(segmentRange.IsCorrect(), ());
  m2::PointD const & ingoingPoint = inEdge.GetStartJunction().GetPoint();
  m2::PointD const & junctionPoint = inEdge.GetEndJunction().GetPoint();

  for (auto const & edge : outgoingEdges)
  {
    if (edge.IsFake())
      continue;

    auto ft = GetFeature(edge.GetFeatureId());
    if (!ft)
      continue;

    feature::TypesHolder types(*ft);

    auto const highwayClass = GetHighwayClass(types);
    ASSERT(highwayClass != HighwayClass::Undefined, (edge.PrintLatLon()));

    double angle = 0;

    if (inEdge.GetFeatureId().m_mwmId == edge.GetFeatureId().m_mwmId)
    {
      ASSERT_LESS(mercator::DistanceOnEarth(junctionPoint, edge.GetStartJunction().GetPoint()),
                  turns::kFeaturesNearTurnMeters, ());
      angle = math::RadToDeg(turns::PiMinusTwoVectorsAngle(junctionPoint, ingoingPoint,
                                                           edge.GetEndJunction().GetPoint()));
    }
    else
    {
      // Note. In case of crossing mwm border
      // (inEdge.GetFeatureId().m_mwmId != edge.GetFeatureId().m_mwmId)
      // twins of inEdge.GetFeatureId() are considered as outgoing features.
      // In this case that turn candidate angle is invalid and
      // should not be used for turn generation.
      outgoingTurns.isCandidatesAngleValid = false;
    }

    outgoingTurns.candidates.emplace_back(angle, ConvertEdgeToSegment(*m_numMwmIds, edge),
                                          highwayClass, m_linkChecker(types));
  }

  if (outgoingTurns.isCandidatesAngleValid)
  {
    sort(outgoingTurns.candidates.begin(), outgoingTurns.candidates.end(),
         base::LessBy(&TurnCandidate::m_angle));
  }
}

void DirectionsEngine::FillPathSegmentsAndAdjacentEdgesMap(
    IndexRoadGraph const & graph, vector<geometry::PointWithAltitude> const & path,
    IRoadGraph::EdgeVector const & routeEdges, base::Cancellable const & cancellable)
{
  size_t const pathSize = path.size();
  CHECK_GREATER(pathSize, 1, ());
  CHECK_EQUAL(routeEdges.size() + 1, pathSize, ());

  // Filling |m_adjacentEdges|.
  auto constexpr kInvalidSegId = numeric_limits<uint32_t>::max();
  // |startSegId| is a value to keep start segment id of a new instance of LoadedPathSegment.
  uint32_t startSegId = kInvalidSegId;

  vector<geometry::PointWithAltitude> prevJunctions;
  vector<Segment> prevSegments;
  for (size_t i = 1; i < pathSize; ++i)
  {
    if (cancellable.IsCancelled())
      return;

    auto const & currJunction = path[i];
    bool const isCurrJunctionFinish = (i + 1 == pathSize);

    IRoadGraph::EdgeListT outgoingEdges, ingoingEdges;
    if (!isCurrJunctionFinish)
      graph.GetOutgoingEdges(currJunction, outgoingEdges);
    graph.GetIngoingEdges(currJunction, ingoingEdges);

    Edge const & inEdge = routeEdges[i - 1];
    uint32_t const inSegId = inEdge.GetSegId();

    if (startSegId == kInvalidSegId)
      startSegId = inSegId;

    prevJunctions.push_back(path[i - 1]);
    prevSegments.push_back(ConvertEdgeToSegment(*m_numMwmIds, inEdge));

    // inEdge.FeatureId may be invalid in case of adding fake features. It happens for example near starts and finishes.
    if (!isCurrJunctionFinish && inEdge.GetFeatureId().IsValid() &&
        !IsJoint(ingoingEdges, outgoingEdges, inEdge, routeEdges[i]))
    {
      continue;
    }

    CHECK_EQUAL(prevJunctions.size(), static_cast<size_t>(abs(int(inSegId) - int(startSegId)) + 1), ());

    prevJunctions.push_back(currJunction);

    AdjacentEdges adjacentEdges(ingoingEdges.size());
    SegmentRange segmentRange;
    GetSegmentRangeAndAdjacentEdges(outgoingEdges, inEdge, startSegId, inSegId, segmentRange,
                                    adjacentEdges.m_outgoingTurns);

    LoadedPathSegment pathSegment;
    pathSegment.m_segmentRange = segmentRange;
    // |prevSegments| contains segments which corresponds to road edges between joints. In case of a
    // fake edge a fake segment is created.
    CHECK_EQUAL(prevSegments.size() + 1, prevJunctions.size(), ());
    pathSegment.m_path = std::move(prevJunctions);
    pathSegment.m_segments = std::move(prevSegments);

    LoadPathAttributes(segmentRange.GetFeature(), pathSegment, inEdge.IsForward());

    if (!segmentRange.IsEmpty())
    {
      /// @todo By VNG: Here was mostly investigational CHECK.
      /// Entry already exists, when start-end points are on the same fake segments.

      //bool const isEmpty = adjacentEdges.m_outgoingTurns.candidates.empty();
      //CHECK(m_adjacentEdges.emplace(segmentRange, std::move(adjacentEdges)).second || isEmpty, ());
      m_adjacentEdges.emplace(segmentRange, std::move(adjacentEdges));
    }

    m_pathSegments.push_back(std::move(pathSegment));

    prevJunctions.clear();
    prevSegments.clear();
    startSegId = kInvalidSegId;
  }
}

bool DirectionsEngine::Generate(IndexRoadGraph const & graph,
                                vector<geometry::PointWithAltitude> const & path,
                                base::Cancellable const & cancellable,
                                vector<RouteSegment> & routeSegments)
{
  CHECK(m_numMwmIds, ());

  m_adjacentEdges.clear();
  m_pathSegments.clear();

  CHECK_NOT_EQUAL(m_vehicleType, VehicleType::Count, (m_vehicleType));

  if (m_vehicleType == VehicleType::Transit)
  {
    auto const & segments = graph.GetRouteSegments();
    uint32_t const segsCount = base::asserted_cast<uint32_t>(segments.size());
    for (uint32_t i = 0; i < segsCount; ++i)
    {
      TurnItem turn;
      if (i == segsCount - 1)
        turn = TurnItem(segsCount, turns::PedestrianDirection::ReachedYourDestination);
      routeSegments.emplace_back(segments[i], turn, path[i + 1], RouteSegment::RoadNameInfo());
    }
    return true;
  }

  if (path.size() <= 1)
    return false;

  IndexRoadGraph::EdgeVector routeEdges;
  graph.GetRouteEdges(routeEdges);

  if (routeEdges.empty())
    return false;

  if (cancellable.IsCancelled())
    return false;

  FillPathSegmentsAndAdjacentEdgesMap(graph, path, routeEdges, cancellable);

  if (cancellable.IsCancelled())
    return false;

  MakeTurnAnnotation(routeEdges, routeSegments);

  CHECK_EQUAL(routeSegments.size(), routeEdges.size(), ());

  return true;
}

/*!
 * \brief Compute turn and time estimation structs for the abstract route result.
 * \param result abstract routing result to annotate.
 * \param delegate Routing callbacks delegate.
 * \param points Storage for unpacked points of the path.
 * \param turnsDir output turns annotation storage.
 * \param streets output street names along the path.
 * \param segments route segments.
 * \return routing operation result code.
 */

// Normally loadedSegments structure is:
// - Start point. Fake loop LoadedPathSegment with 1 segment of zero length.
// - Straight jump from start point to the beginning of real route. LoadedPathSegment with 1 segment.
// - Real route. N LoadedPathSegments, each with arbitrary amount of segments. N >= 1.
// - Straight jump from the end of real route to finish point. LoadedPathSegment with 1 segment.
// - Finish point. Fake loop LoadedPathSegment with 1 segment of zero length.
// So minimal amount of segments is 5.

// Resulting structure of turnsDir:
// No Turn for 0th segment (no ingoing). m_index == 0.
// No Turn for 1st segment (ingoing fake loop) - at start point. m_index == 1.
// No Turn for 2nd (ingoing == jump) - at beginning of real route. m_index == 2.
// Possible turn for next N-1 segments. m_index >= 3.
// No Turn for (2 + N + 1)th segment (outgoing jump) - at finish point. m_index = 3 + M.
// No Turn for (2 + N + 2)th segment (outgoing fake loop) - at finish point. m_index == 4 + M.
// Added ReachedYourDestination - at finish point. m_index == 4 + M.
// Where M - total amount of all segments from all LoadedPathSegments (size of |segments|).
// So minimum m_index of ReachedYourDestination is 5 (real route with single segment),
// and minimal |turnsDir| is - single ReachedYourDestination with m_index == 5.

void DirectionsEngine::MakeTurnAnnotation(IndexRoadGraph::EdgeVector const & routeEdges,
                                          vector<RouteSegment> & routeSegments)
{
  CHECK_GREATER_OR_EQUAL(routeEdges.size(), 2, ());

  RoutingEngineResult result(routeEdges, m_adjacentEdges, m_pathSegments);

  routeSegments.reserve(routeEdges.size());

  RoutingSettings const vehicleSettings = GetRoutingSettings(m_vehicleType);

  auto const & loadedSegments = result.GetSegments(); // the same as m_pathSegments

  // First point of first loadedSegment is ignored. This is the reason for:
  //ASSERT_EQUAL(loadedSegments.front().m_path.back(), loadedSegments.front().m_path.front(), ());

  size_t skipTurnSegments = 0;
  for (size_t idxLoadedSegment = 0; idxLoadedSegment < loadedSegments.size(); ++idxLoadedSegment)
  {
    auto const & loadedSegment = loadedSegments[idxLoadedSegment];

    CHECK(loadedSegment.IsValid(), ());
    CHECK_GREATER_OR_EQUAL(loadedSegment.m_segments.size(), 1, ());
    CHECK_EQUAL(loadedSegment.m_segments.size() + 1, loadedSegment.m_path.size(), ());

    auto rni = loadedSegment.m_roadNameInfo;

    for (size_t i = 0; i < loadedSegment.m_segments.size() - 1; ++i)
    {
      auto const & junction = loadedSegment.m_path[i + 1];
      routeSegments.emplace_back(loadedSegment.m_segments[i], TurnItem(), junction, rni);
      if (i == 0)
        rni = {"","","","", "", loadedSegment.m_isLink};
    }

    // For the last segment of current loadedSegment put info about turn
    // from current loadedSegment to the next one.
    TurnItem turnItem;
    if (skipTurnSegments == 0)
    {
      turnItem.m_index = base::asserted_cast<uint32_t>(routeSegments.size() + 1);
      skipTurnSegments = GetTurnDirection(result, idxLoadedSegment + 1, *m_numMwmIds, vehicleSettings, turnItem);
    }
    else
      --skipTurnSegments;

    routeSegments.emplace_back(loadedSegment.m_segments.back(), turnItem, loadedSegment.m_path.back(), rni);
  }

  //ASSERT_EQUAL(routeSegments.front().GetJunction(), result.GetStartPoint(), ());
  //ASSERT_EQUAL(routeSegments.back().GetJunction(), result.GetEndPoint(), ());

  FixupTurns(routeSegments);
}

}  // namespace routing
