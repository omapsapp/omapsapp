#include "routing/single_vehicle_world_graph.hpp"

#include "base/assert.hpp"

#include <algorithm>

namespace routing
{
using namespace std;

template <>
SingleVehicleWorldGraph::AStarParents<Segment>::ParentType
SingleVehicleWorldGraph::AStarParents<Segment>::kEmpty = {};

template <>
SingleVehicleWorldGraph::AStarParents<JointSegment>::ParentType
SingleVehicleWorldGraph::AStarParents<JointSegment>::kEmpty = {};

SingleVehicleWorldGraph::SingleVehicleWorldGraph(unique_ptr<CrossMwmGraph> crossMwmGraph,
                                                 unique_ptr<IndexGraphLoader> loader,
                                                 shared_ptr<EdgeEstimator> estimator,
                                                 MwmHierarchyHandler && hierarchyHandler)
  : m_crossMwmGraph(std::move(crossMwmGraph))
  , m_loader(std::move(loader))
  , m_estimator(std::move(estimator))
  , m_hierarchyHandler(std::move(hierarchyHandler))
{
  CHECK(m_loader, ());
  CHECK(m_estimator, ());
}

void SingleVehicleWorldGraph::CheckAndProcessTransitFeatures(Segment const & parent,
                                                             JointEdgeListT & jointEdges,
                                                             WeightListT & parentWeights,
                                                             bool isOutgoing)
{
  JointEdgeListT newCrossMwmEdges;

  NumMwmId const mwmId = parent.GetMwmId();

  for (size_t i = 0; i < jointEdges.size(); ++i)
  {
    JointSegment const & target = jointEdges[i].GetTarget();

    vector<Segment> twins;
    m_crossMwmGraph->GetTwinFeature(target.GetSegment(true /* start */), isOutgoing, twins);
    if (twins.empty())
    {
      ASSERT_EQUAL(mwmId, target.GetMwmId(), ());
      continue;
    }

    auto & currentIndexGraph = GetIndexGraph(mwmId);
    for (auto const & twin : twins)
    {
      NumMwmId const twinMwmId = twin.GetMwmId();
      Segment const start(twinMwmId, twin.GetFeatureId(), target.GetSegmentId(isOutgoing), target.IsForward());

      auto & twinIndexGraph = GetIndexGraph(twinMwmId);

      IndexGraph::PointIdListT lastPoints;
      twinIndexGraph.GetLastPointsForJoint({start}, isOutgoing, lastPoints);
      ASSERT_EQUAL(lastPoints.size(), 1, ());

      if (auto edge = currentIndexGraph.GetJointEdgeByLastPoint(parent,
                                                                target.GetSegment(isOutgoing),
                                                                isOutgoing, lastPoints.back()))
      {
        newCrossMwmEdges.emplace_back(*edge);
        newCrossMwmEdges.back().GetTarget().AssignID(twin);

        parentWeights.push_back(parentWeights[i]);
      }
    }
  }

  jointEdges.insert(jointEdges.end(), newCrossMwmEdges.begin(), newCrossMwmEdges.end());
}

void SingleVehicleWorldGraph::GetEdgeList(astar::VertexData<Segment, RouteWeight> const & vertexData, bool isOutgoing,
                                          bool useRoutingOptions, bool, SegmentEdgeListT & edges)
{
  CHECK_NOT_EQUAL(m_mode, WorldGraphMode::LeapsOnly, ());

  ASSERT(m_parentsForSegments.forward && m_parentsForSegments.backward,
         ("m_parentsForSegments was not initialized in SingleVehicleWorldGraph."));
  auto const & segment = vertexData.m_vertex;
  auto & parents = isOutgoing ? *m_parentsForSegments.forward : *m_parentsForSegments.backward;
  IndexGraph & indexGraph = m_loader->GetIndexGraph(segment.GetMwmId());
  indexGraph.GetEdgeList(vertexData, isOutgoing, useRoutingOptions, edges, parents);

  if (m_mode != WorldGraphMode::SingleMwm && m_crossMwmGraph && m_crossMwmGraph->IsTransition(segment, isOutgoing))
    GetTwins(segment, isOutgoing, useRoutingOptions, edges);
}

void SingleVehicleWorldGraph::GetEdgeList(astar::VertexData<JointSegment, RouteWeight> const & parentVertexData,
                                          Segment const & parent, bool isOutgoing, bool, JointEdgeListT & jointEdges,
                                          WeightListT & parentWeights)
{
  // Fake segments aren't processed here. All work must be done
  // on the IndexGraphStarterJoints abstraction-level.
  if (!parent.IsRealSegment())
    return;

  ASSERT(m_parentsForJoints.forward && m_parentsForJoints.backward,
         ("m_parentsForJoints was not initialized in SingleVehicleWorldGraph."));
  auto const & parents = isOutgoing ? *m_parentsForJoints.forward : *m_parentsForJoints.backward;
  auto & indexGraph = GetIndexGraph(parent.GetMwmId());
  indexGraph.GetEdgeList(parentVertexData, parent, isOutgoing, jointEdges, parentWeights, parents);

  // Comment this call to debug routing on single generated mwm.
  if (m_mode != WorldGraphMode::JointSingleMwm)
    CheckAndProcessTransitFeatures(parent, jointEdges, parentWeights, isOutgoing);

  ASSERT_EQUAL(jointEdges.size(), parentWeights.size(), ());
}

LatLonWithAltitude const & SingleVehicleWorldGraph::GetJunction(Segment const & segment, bool front)
{
  return GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId())
         .GetJunction(segment.GetPointId(front));
}

ms::LatLon const & SingleVehicleWorldGraph::GetPoint(Segment const & segment, bool front)
{
  return GetJunction(segment, front).GetLatLon();
}

bool SingleVehicleWorldGraph::IsOneWay(NumMwmId mwmId, uint32_t featureId)
{
  return GetRoadGeometry(mwmId, featureId).IsOneWay();
}

bool SingleVehicleWorldGraph::IsPassThroughAllowed(NumMwmId mwmId, uint32_t featureId)
{
  return GetRoadGeometry(mwmId, featureId).IsPassThroughAllowed();
}

RouteWeight SingleVehicleWorldGraph::HeuristicCostEstimate(ms::LatLon const & from,
                                                           ms::LatLon const & to)
{
  return RouteWeight(m_estimator->CalcHeuristic(from, to));
}


RouteWeight SingleVehicleWorldGraph::CalcSegmentWeight(Segment const & segment,
                                                       EdgeEstimator::Purpose purpose)
{
  return RouteWeight(m_estimator->CalcSegmentWeight(
      segment, GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId()), purpose));
}

RouteWeight SingleVehicleWorldGraph::CalcLeapWeight(ms::LatLon const & from,
                                                    ms::LatLon const & to,
                                                    NumMwmId mwmId) const
{
  return RouteWeight(m_estimator->CalcLeapWeight(from, to, mwmId));
}

RouteWeight SingleVehicleWorldGraph::CalcOffroadWeight(ms::LatLon const & from,
                                                       ms::LatLon const & to,
                                                       EdgeEstimator::Purpose purpose) const
{
  return RouteWeight(m_estimator->CalcOffroad(from, to, purpose));
}

double SingleVehicleWorldGraph::CalculateETA(Segment const & from, Segment const & to)
{
  /// @todo Crutch, for example we can loose ferry penalty here (no twin segments), @see Russia_CrossMwm_Ferry.
  if (from.GetMwmId() != to.GetMwmId())
    return CalculateETAWithoutPenalty(to);

  auto & indexGraph = m_loader->GetIndexGraph(from.GetMwmId());
  return indexGraph.CalculateEdgeWeight(EdgeEstimator::Purpose::ETA, true /* isOutgoing */, from, to).GetWeight();
}

double SingleVehicleWorldGraph::CalculateETAWithoutPenalty(Segment const & segment)
{
  return m_estimator->CalcSegmentWeight(segment,
                                        GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId()),
                                        EdgeEstimator::Purpose::ETA);
}

void SingleVehicleWorldGraph::ForEachTransition(NumMwmId numMwmId, bool isEnter, TransitionFnT const & fn)
{
  return m_crossMwmGraph->ForEachTransition(numMwmId, isEnter, fn);
}

vector<RouteSegment::SpeedCamera> SingleVehicleWorldGraph::GetSpeedCamInfo(Segment const & segment)
{
  ASSERT(segment.IsRealSegment(), ());
  return m_loader->GetSpeedCameraInfo(segment);
}

SpeedInUnits SingleVehicleWorldGraph::GetSpeedLimit(Segment const & segment)
{
  ASSERT(segment.IsRealSegment(), ());
  return GetIndexGraph(segment.GetMwmId()).GetGeometry().GetSavedMaxspeed(segment.GetFeatureId(), segment.IsForward());
}

RoadGeometry const & SingleVehicleWorldGraph::GetRoadGeometry(NumMwmId mwmId, uint32_t featureId)
{
  return m_loader->GetGeometry(mwmId).GetRoad(featureId);
}

void SingleVehicleWorldGraph::GetTwinsInner(Segment const & segment, bool isOutgoing,
                                            vector<Segment> & twins)
{
  m_crossMwmGraph->GetTwins(segment, isOutgoing, twins);
}

RouteWeight SingleVehicleWorldGraph::GetCrossBorderPenalty(NumMwmId mwmId1, NumMwmId mwmId2)
{
  return m_hierarchyHandler.GetCrossBorderPenalty(mwmId1, mwmId2);
}

bool SingleVehicleWorldGraph::IsRoutingOptionsGood(Segment const & segment)
{
  auto const & geometry = GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId());
  return geometry.SuitableForOptions(m_avoidRoutingOptions);
}

RoutingOptions SingleVehicleWorldGraph::GetRoutingOptions(Segment const & segment)
{
  ASSERT(segment.IsRealSegment(), ());

  auto const & geometry = GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId());
  return geometry.GetRoutingOptions();
}

void SingleVehicleWorldGraph::SetAStarParents(bool forward, Parents<Segment> & parents)
{
  if (forward)
    m_parentsForSegments.forward = &parents;
  else
    m_parentsForSegments.backward = &parents;
}

void SingleVehicleWorldGraph::SetAStarParents(bool forward, Parents<JointSegment> & parents)
{
  if (forward)
    m_parentsForJoints.forward = &parents;
  else
    m_parentsForJoints.backward = &parents;
}

void SingleVehicleWorldGraph::DropAStarParents()
{
  m_parentsForJoints.backward = &AStarParents<JointSegment>::kEmpty;
  m_parentsForJoints.forward = &AStarParents<JointSegment>::kEmpty;

  m_parentsForSegments.backward = &AStarParents<Segment>::kEmpty;
  m_parentsForSegments.forward = &AStarParents<Segment>::kEmpty;
}

template <typename VertexType>
NumMwmId GetCommonMwmInChain(vector<VertexType> const & chain)
{
  NumMwmId mwmId = kFakeNumMwmId;
  for (size_t i = 0; i < chain.size(); ++i)
  {
    if (chain[i].GetMwmId() == kFakeNumMwmId)
      continue;

    if (mwmId != kFakeNumMwmId && mwmId != chain[i].GetMwmId())
      return kFakeNumMwmId;

    mwmId = chain[i].GetMwmId();
  }

  return mwmId;
}

template <typename VertexType, class ConverterT> bool
SingleVehicleWorldGraph::AreWavesConnectibleImpl(Parents<VertexType> const & forwardParents,
                                                 VertexType const & commonVertex,
                                                 Parents<VertexType> const & backwardParents,
                                                 ConverterT const & fakeConverter)
{
  if (IsRegionsGraphMode())
    return true;

  vector<VertexType> chain;
  auto const fillUntilNextFeatureId = [&](VertexType const & cur, auto const & parents)
  {
    auto startFeatureId = cur.GetFeatureId();
    auto it = parents.find(cur);
    while (it != parents.end() && it->second.GetFeatureId() == startFeatureId)
    {
      chain.emplace_back(it->second);
      it = parents.find(it->second);
    }

    if (it == parents.end())
      return false;

    chain.emplace_back(it->second);
    return true;
  };

  auto const fillParents = [&](VertexType current, auto const & parents)
  {
    /// @todo Realize the meaning of this constant.
    static uint32_t constexpr kStepCountOneSide = 3;

    for (uint32_t i = 0; i < kStepCountOneSide; ++i)
    {
      if (!fillUntilNextFeatureId(current, parents))
        break;

      current = chain.back();
    }
  };

  fillParents(commonVertex, forwardParents);

  reverse(chain.begin(), chain.end());
  chain.emplace_back(commonVertex);

  fillParents(commonVertex, backwardParents);

  fakeConverter(chain);

  NumMwmId const mwmId = GetCommonMwmInChain(chain);
  if (mwmId == kFakeNumMwmId)
    return true;

  Parents<VertexType> parents;
  for (size_t i = 1; i < chain.size(); ++i)
    parents[chain[i]] = chain[i - 1];

  auto & currentIndexGraph = GetIndexGraph(mwmId);
  for (size_t i = 1; i < chain.size(); ++i)
  {
    auto const & parent = chain[i - 1];
    uint32_t const parentFeatureId = chain[i - 1].GetFeatureId();
    uint32_t const currentFeatureId = chain[i].GetFeatureId();

    if (parentFeatureId != currentFeatureId &&
        currentIndexGraph.IsRestricted(parent, parentFeatureId, currentFeatureId,
                                       true /* isOutgoing */, parents))
    {
      return false;
    }
  }

  return true;
}

bool SingleVehicleWorldGraph::AreWavesConnectible(Parents<Segment> & forwardParents,
                                                  Segment const & commonVertex,
                                                  Parents<Segment> & backwardParents)
{
  return AreWavesConnectibleImpl(forwardParents, commonVertex, backwardParents,
                                 [](vector<Segment> &) {});
}

bool SingleVehicleWorldGraph::AreWavesConnectible(Parents<JointSegment> & forwardParents,
                                                  JointSegment const & commonVertex,
                                                  Parents<JointSegment> & backwardParents,
                                                  FakeConverterT const & fakeFeatureConverter)
{
  return AreWavesConnectibleImpl(forwardParents, commonVertex, backwardParents,
                                 [&fakeFeatureConverter](vector<JointSegment> & chain)
  {
    for (auto & vertex : chain)
      fakeFeatureConverter(vertex);
  });
}
}  // namespace routing
