#include "routing/index_graph.hpp"

#include "routing/restrictions_serialization.hpp"
#include "routing/routing_options.hpp"
#include "routing/world_graph.hpp"

#include "platform/settings.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/exception.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <utility>

namespace routing
{
using namespace base;
using namespace std;

bool IsUTurn(Segment const & u, Segment const & v)
{
  return u.GetFeatureId() == v.GetFeatureId() && u.GetSegmentIdx() == v.GetSegmentIdx() &&
         u.IsForward() != v.IsForward();
}

bool IsBoarding(bool prevIsFerry, bool nextIsFerry)
{
  return !prevIsFerry && nextIsFerry;
}

IndexGraph::IndexGraph(shared_ptr<Geometry> geometry, shared_ptr<EdgeEstimator> estimator,
                       RoutingOptions routingOptions)
  : m_geometry(std::move(geometry)),
    m_estimator(std::move(estimator)),
    m_avoidRoutingOptions(routingOptions)
{
  CHECK(m_geometry, ());
  CHECK(m_estimator, ());
}

bool IndexGraph::IsJoint(RoadPoint const & roadPoint) const
{
  return m_roadIndex.GetJointId(roadPoint) != Joint::kInvalidId;
}

bool IndexGraph::IsJointOrEnd(Segment const & segment, bool fromStart) const
{
  if (IsJoint(segment.GetRoadPoint(fromStart)))
    return true;

  // For features, that ends out of mwm. In this case |m_graph.IsJoint| returns false, but we should
  // think, that it's Joint anyway.
  uint32_t const pointId = segment.GetPointId(fromStart);
  if (pointId == 0)
    return true;

  uint32_t const pointsNumber = GetRoadGeometry(segment.GetFeatureId()).GetPointsCount();
  return pointId + 1 == pointsNumber;
}

void IndexGraph::GetEdgeList(astar::VertexData<Segment, RouteWeight> const & vertexData,
                             bool isOutgoing, bool useRoutingOptions, SegmentEdgeListT & edges,
                             Parents<Segment> const & parents) const
{
  GetEdgeListImpl(vertexData, isOutgoing, useRoutingOptions, true /* useAccessConditional */, edges,
                  parents);
}

void IndexGraph::GetEdgeList(Segment const & segment,
                             bool isOutgoing, bool useRoutingOptions, SegmentEdgeListT & edges,
                             Parents<Segment> const & parents) const
{
  GetEdgeListImpl({segment, Weight(0.0)} /* vertexData */, isOutgoing, useRoutingOptions,
                  false /* useAccessConditional */, edges, parents);
}

void IndexGraph::GetEdgeListImpl(astar::VertexData<Segment, RouteWeight> const & vertexData,
                                 bool isOutgoing, bool useRoutingOptions, bool useAccessConditional,
                                 SegmentEdgeListT & edges, Parents<Segment> const & parents) const
{
  auto const & segment = vertexData.m_vertex;

  RoadPoint const roadPoint = segment.GetRoadPoint(isOutgoing);
  Joint::Id const jointId = m_roadIndex.GetJointId(roadPoint);

  if (jointId != Joint::kInvalidId)
  {
    m_jointIndex.ForEachPoint(jointId, [&](RoadPoint const & rp) {
      GetNeighboringEdges(vertexData, rp, isOutgoing, useRoutingOptions, edges, parents,
                          useAccessConditional);
    });
  }
  else
  {
    GetNeighboringEdges(vertexData, roadPoint, isOutgoing, useRoutingOptions, edges, parents,
                        useAccessConditional);
  }
}

void IndexGraph::GetLastPointsForJoint(SegmentListT const & children,
                                       bool isOutgoing,
                                       PointIdListT & lastPoints) const
{
  ASSERT(lastPoints.empty(), ());

  lastPoints.reserve(children.size());
  for (auto const & child : children)
  {
    uint32_t const startPointId = child.GetPointId(!isOutgoing /* front */);
    uint32_t const pointsNumber = GetRoadGeometry(child.GetFeatureId()).GetPointsCount();
    CHECK_LESS(startPointId, pointsNumber, (child));

    uint32_t endPointId;
    // child.IsForward() == isOutgoing
    //   This is the direction, indicating the end of the road,
    //   where we should go for building JointSegment.
    // You can retrieve such result if bust possible options of |child.IsForward()| and |isOutgoing|.
    bool forward = child.IsForward() == isOutgoing;
    if (IsRoad(child.GetFeatureId()))
    {
      endPointId = GetRoad(child.GetFeatureId()).FindNeighbor(startPointId, forward, pointsNumber).second;
    }
    else
    {
      // child.GetFeatureId() can be not road in this case:
      // -->--> { -->-->--> } -->
      // Where { ... } - borders of mwm
      // Here one feature, which enter from mwm A to mwm B and then exit from B to A.
      // And in mwm B no other feature cross this one, those feature is in geometry
      // and absent in roadIndex.
      // In this case we just return endPointNumber.
      endPointId = forward ? pointsNumber - 1 : 0;
    }

    lastPoints.push_back(endPointId);
  }
}

void IndexGraph::GetEdgeList(astar::VertexData<JointSegment, RouteWeight> const & parentVertexData,
                             Segment const & parent, bool isOutgoing,
                             JointEdgeListT & edges,
                             WeightListT & parentWeights,
                             Parents<JointSegment> const & parents) const
{
  GetEdgeListImpl(parentVertexData, parent, isOutgoing, true /* useAccessConditional */, edges,
                  parentWeights, parents);
}

void IndexGraph::GetEdgeList(JointSegment const & parentJoint, Segment const & parent,
                             bool isOutgoing, JointEdgeListT & edges,
                             WeightListT & parentWeights,
                             Parents<JointSegment> const & parents) const
{
  GetEdgeListImpl({parentJoint, Weight(0.0)}, parent, isOutgoing, false /* useAccessConditional */, edges,
                  parentWeights, parents);
}

void IndexGraph::GetEdgeListImpl(astar::VertexData<JointSegment, RouteWeight> const & parentVertexData,
                                 Segment const & parent, bool isOutgoing, bool, JointEdgeListT & edges,
                                 WeightListT & parentWeights, Parents<JointSegment> const & parents) const
{
  SegmentListT possibleChildren;
  GetSegmentCandidateForJoint(parent, isOutgoing, possibleChildren);

  PointIdListT lastPoints;
  GetLastPointsForJoint(possibleChildren, isOutgoing, lastPoints);

  ReconstructJointSegment(parentVertexData, parent, possibleChildren, lastPoints,
                          isOutgoing, edges, parentWeights, parents);
}

optional<JointEdge> IndexGraph::GetJointEdgeByLastPoint(Segment const & parent,
                                                        Segment const & firstChild, bool isOutgoing,
                                                        uint32_t lastPoint) const
{
  SegmentListT const possibleChildren = {firstChild};
  PointIdListT const lastPoints = {lastPoint};

  JointEdgeListT edges;
  WeightListT parentWeights;
  Parents<JointSegment> emptyParents;
  ReconstructJointSegment(astar::VertexData<JointSegment, RouteWeight>::Zero(), parent, possibleChildren, lastPoints,
                          isOutgoing, edges, parentWeights, emptyParents);

  CHECK_LESS_OR_EQUAL(edges.size(), 1, ());
  if (edges.size() == 1)
    return {edges.back()};

  return {};
}

void IndexGraph::Build(uint32_t numJoints)
{
  m_jointIndex.Build(m_roadIndex, numJoints);
}

void IndexGraph::Import(vector<Joint> const & joints)
{
  m_roadIndex.Import(joints);
  CHECK_LESS_OR_EQUAL(joints.size(), numeric_limits<uint32_t>::max(), ());
  Build(checked_cast<uint32_t>(joints.size()));
}

void IndexGraph::SetRestrictions(RestrictionVec && restrictions)
{
  m_restrictionsForward.clear();
  m_restrictionsBackward.clear();

  base::HighResTimer timer;
  for (auto const & restriction : restrictions)
  {
    ASSERT(!restriction.empty(), ());

    auto & forward = m_restrictionsForward[restriction.back()];
    forward.emplace_back(restriction.begin(), prev(restriction.end()));
    reverse(forward.back().begin(), forward.back().end());

    m_restrictionsBackward[restriction.front()].emplace_back(next(restriction.begin()), restriction.end());
  }

  LOG(LDEBUG, ("Restrictions are loaded in:", timer.ElapsedMilliseconds(), "ms"));
}

void IndexGraph::SetUTurnRestrictions(vector<RestrictionUTurn> && noUTurnRestrictions)
{
  for (auto const & noUTurn : noUTurnRestrictions)
  {
    if (noUTurn.m_viaIsFirstPoint)
      m_noUTurnRestrictions[noUTurn.m_featureId].m_atTheBegin = true;
    else
      m_noUTurnRestrictions[noUTurn.m_featureId].m_atTheEnd = true;
  }
}

void IndexGraph::SetRoadAccess(RoadAccess && roadAccess)
{
  m_roadAccess = std::move(roadAccess);
  m_roadAccess.SetCurrentTimeGetter(m_currentTimeGetter);
}

void IndexGraph::GetNeighboringEdges(astar::VertexData<Segment, RouteWeight> const & fromVertexData,
                                     RoadPoint const & rp, bool isOutgoing, bool useRoutingOptions,
                                     SegmentEdgeListT & edges, Parents<Segment> const & parents,
                                     bool useAccessConditional) const
{
  RoadGeometry const & road = GetRoadGeometry(rp.GetFeatureId());

  if (!road.IsValid())
    return;

  if (useRoutingOptions && !road.SuitableForOptions(m_avoidRoutingOptions))
    return;

  bool const bidirectional = !road.IsOneWay();
  auto const & from = fromVertexData.m_vertex;
  if ((isOutgoing || bidirectional) && rp.GetPointId() + 1 < road.GetPointsCount())
  {
    GetNeighboringEdge(fromVertexData,
                       Segment(from.GetMwmId(), rp.GetFeatureId(), rp.GetPointId(), isOutgoing),
                       isOutgoing, edges, parents, useAccessConditional);
  }

  if ((!isOutgoing || bidirectional) && rp.GetPointId() > 0)
  {
    GetNeighboringEdge(
        fromVertexData,
        Segment(from.GetMwmId(), rp.GetFeatureId(), rp.GetPointId() - 1, !isOutgoing), isOutgoing,
        edges, parents, useAccessConditional);
  }
}

void IndexGraph::GetSegmentCandidateForRoadPoint(RoadPoint const & rp, NumMwmId numMwmId,
                                                 bool isOutgoing, SegmentListT & children) const
{
  RoadGeometry const & road = GetRoadGeometry(rp.GetFeatureId());
  if (!road.IsValid())
    return;

  if (!road.SuitableForOptions(m_avoidRoutingOptions))
    return;

  bool const bidirectional = !road.IsOneWay();
  auto const pointId = rp.GetPointId();

  if ((isOutgoing || bidirectional) && pointId + 1 < road.GetPointsCount())
    children.emplace_back(numMwmId, rp.GetFeatureId(), pointId, isOutgoing);

  if ((!isOutgoing || bidirectional) && pointId > 0)
    children.emplace_back(numMwmId, rp.GetFeatureId(), pointId - 1, !isOutgoing);
}

void IndexGraph::GetSegmentCandidateForJoint(Segment const & parent, bool isOutgoing,
                                             SegmentListT & children) const
{
  RoadPoint const roadPoint = parent.GetRoadPoint(isOutgoing);
  Joint::Id const jointId = m_roadIndex.GetJointId(roadPoint);

  if (jointId == Joint::kInvalidId)
    return;

  m_jointIndex.ForEachPoint(jointId, [&](RoadPoint const & rp) {
    GetSegmentCandidateForRoadPoint(rp, parent.GetMwmId(), isOutgoing, children);
  });
}

/// \brief Prolongs segments from |parent| to |firstChildren| directions in order to
///        create JointSegments.
/// \param |firstChildren| - vector of neighbouring segments from parent.
/// \param |lastPointIds| - vector of the end numbers of road points for |firstChildren|.
/// \param |jointEdges| - the result vector with JointEdges.
/// \param |parentWeights| - see |IndexGraphStarterJoints::GetEdgeList| method about this argument.
///                          Shortly - in case of |isOutgoing| == false, method saves here the weights
///                                   from parent to firstChildren.
void IndexGraph::ReconstructJointSegment(astar::VertexData<JointSegment, RouteWeight> const & parentVertexData,
                                         Segment const & parent,
                                         SegmentListT const & firstChildren,
                                         PointIdListT const & lastPointIds,
                                         bool isOutgoing,
                                         JointEdgeListT & jointEdges,
                                         WeightListT & parentWeights,
                                         Parents<JointSegment> const & parents) const
{
  CHECK_EQUAL(firstChildren.size(), lastPointIds.size(), ());

  auto const & weightTimeToParent = parentVertexData.m_realDistance;
  auto const & parentJoint = parentVertexData.m_vertex;
  for (size_t i = 0; i < firstChildren.size(); ++i)
  {
    auto const & firstChild = firstChildren[i];
    auto const lastPointId = lastPointIds[i];

    uint32_t currentPointId = firstChild.GetPointId(!isOutgoing /* front */);
    CHECK_NOT_EQUAL(currentPointId, lastPointId,
                    ("Invariant violated, can not build JointSegment,"
                     "started and ended in the same point."));

    auto const increment = [currentPointId, lastPointId](uint32_t pointId) {
      return currentPointId < lastPointId ? pointId + 1 : pointId - 1;
    };

    if (IsAccessNoForSure(firstChild.GetFeatureId(), weightTimeToParent,
                          true /* useAccessConditional */))
    {
      continue;
    }

    if (IsAccessNoForSure(parent.GetRoadPoint(isOutgoing), weightTimeToParent,
                          true /* useAccessConditional */))
    {
      continue;
    }

    if (IsUTurn(parent, firstChild) && IsUTurnAndRestricted(parent, firstChild, isOutgoing))
      continue;

    if (IsRestricted(parentJoint, parent.GetFeatureId(), firstChild.GetFeatureId(),
                     isOutgoing, parents))
    {
      continue;
    }

    RouteWeight summaryWeight;
    // Check current JointSegment for bad road access between segments.
    RoadPoint rp = firstChild.GetRoadPoint(isOutgoing);
    uint32_t start = currentPointId;
    bool noRoadAccess = false;
    do
    {
      // This is optimization: we calculate accesses of road points before calculating weight of
      // segments between these road points. Because of that we make less calculations when some
      // points have RoadAccess::Type::No.
      // And using |weightTimeToParent| is not fair in fact, because we should calculate weight
      // until this |rp|. But we assume that segments have small length and inaccuracy will not
      // affect user.
      if (IsAccessNoForSure(rp, weightTimeToParent, true /* useAccessConditional */))
      {
        noRoadAccess = true;
        break;
      }

      start = increment(start);
      rp.SetPointId(start);
    } while (start != lastPointId);

    if (noRoadAccess)
      continue;

    bool forward = currentPointId < lastPointId;
    Segment current = firstChild;
    Segment prev = parent;

    do
    {
      RouteWeight const weight = CalculateEdgeWeight(EdgeEstimator::Purpose::Weight, isOutgoing,
                                                     prev, current, weightTimeToParent);

      if (isOutgoing || prev != parent)
        summaryWeight += weight;

      if (prev == parent)
        parentWeights.emplace_back(weight);

      prev = current;
      current.Next(forward);
      currentPointId = increment(currentPointId);
    } while (currentPointId != lastPointId);

    jointEdges.emplace_back(isOutgoing ? JointSegment(firstChild, prev) :
                                         JointSegment(prev, firstChild),
                            summaryWeight);
  }
}

void IndexGraph::GetNeighboringEdge(astar::VertexData<Segment, RouteWeight> const & fromVertexData,
                                    Segment const & to, bool isOutgoing,
                                    SegmentEdgeListT & edges, Parents<Segment> const & parents,
                                    bool useAccessConditional) const
{
  auto const & from = fromVertexData.m_vertex;
  auto const & weightToFrom = fromVertexData.m_realDistance;

  if (IsUTurn(from, to) && IsUTurnAndRestricted(from, to, isOutgoing))
    return;

  if (IsRestricted(from, from.GetFeatureId(), to.GetFeatureId(), isOutgoing, parents))
    return;

  if (IsAccessNoForSure(to.GetFeatureId(), weightToFrom, useAccessConditional))
    return;

  RoadPoint const rp = from.GetRoadPoint(isOutgoing);
  if (IsAccessNoForSure(rp, weightToFrom, useAccessConditional))
    return;

  auto const weight =
      CalculateEdgeWeight(EdgeEstimator::Purpose::Weight, isOutgoing, from, to, weightToFrom);

  edges.emplace_back(to, weight);
}

IndexGraph::PenaltyData IndexGraph::GetRoadPenaltyData(Segment const & segment) const
{
  auto const & road = GetRoadGeometry(segment.GetFeatureId());
  return { road.IsPassThroughAllowed(), road.GetRoutingOptions().Has(RoutingOptions::Road::Ferry) };
}

RouteWeight IndexGraph::GetPenalties(EdgeEstimator::Purpose purpose, Segment const & u,
                                     Segment const & v, optional<RouteWeight> const & prevWeight) const
{
  auto const & fromPenaltyData = GetRoadPenaltyData(u);
  auto const & toPenaltyData = GetRoadPenaltyData(v);
  // Route crosses border of pass-through/non-pass-through area if |u| and |v| have different
  // pass through restrictions.
  int8_t const passThroughPenalty =
      fromPenaltyData.m_passThroughAllowed == toPenaltyData.m_passThroughAllowed ? 0 : 1;

  int8_t accessPenalty = 0;
  int8_t accessConditionalPenalties = 0;

  if (u.GetFeatureId() != v.GetFeatureId())
  {
    // We do not distinguish between RoadAccess::Type::Private and RoadAccess::Type::Destination for
    // now.
    auto const [fromAccess, fromConfidence] =
        prevWeight ? m_roadAccess.GetAccess(u.GetFeatureId(), *prevWeight)
                   : m_roadAccess.GetAccessWithoutConditional(u.GetFeatureId());

    auto const [toAccess, toConfidence] =
        prevWeight ? m_roadAccess.GetAccess(v.GetFeatureId(), *prevWeight)
                   : m_roadAccess.GetAccessWithoutConditional(v.GetFeatureId());

    if (fromConfidence == RoadAccess::Confidence::Sure &&
        toConfidence == RoadAccess::Confidence::Sure)
    {
      bool const fromAccessAllowed = fromAccess == RoadAccess::Type::Yes;
      bool const toAccessAllowed = toAccess == RoadAccess::Type::Yes;
      // Route crosses border of access=yes/access={private, destination} area if |u| and |v| have
      // different access restrictions.
      accessPenalty = fromAccessAllowed == toAccessAllowed ? 0 : 1;
    }
    else if (toConfidence == RoadAccess::Confidence::Maybe)
    {
      accessConditionalPenalties = 1;
    }
  }

  // RoadPoint between u and v is front of u.
  auto const rp = u.GetRoadPoint(true /* front */);
  auto const [rpAccessType, rpConfidence] = prevWeight
                                                ? m_roadAccess.GetAccess(rp, *prevWeight)
                                                : m_roadAccess.GetAccessWithoutConditional(rp);
  switch (rpConfidence)
  {
  case RoadAccess::Confidence::Sure:
  {
    if (rpAccessType != RoadAccess::Type::Yes)
      accessPenalty = 1;
    break;
  }
  case RoadAccess::Confidence::Maybe:
  {
    accessConditionalPenalties = 1;
    break;
  }
  }

  double weightPenalty = 0.0;
  if (IsUTurn(u, v))
    weightPenalty += m_estimator->GetUTurnPenalty(purpose);

  if (IsBoarding(fromPenaltyData.m_isFerry, toPenaltyData.m_isFerry))
    weightPenalty += m_estimator->GetFerryLandingPenalty(purpose);

  return {weightPenalty /* weight */, passThroughPenalty, accessPenalty, accessConditionalPenalties,
          0.0 /* transitTime */};
}

WorldGraphMode IndexGraph::GetMode() const { return WorldGraphMode::Undefined; }

bool IndexGraph::IsUTurnAndRestricted(Segment const & parent, Segment const & child,
                                      bool isOutgoing) const
{
  ASSERT(IsUTurn(parent, child), ());

  uint32_t const featureId = parent.GetFeatureId();
  uint32_t const turnPoint = parent.GetPointId(isOutgoing);
  auto const & roadGeometry = GetRoadGeometry(featureId);

  RoadPoint const rp = parent.GetRoadPoint(isOutgoing);
  if (m_roadIndex.GetJointId(rp) == Joint::kInvalidId && !roadGeometry.IsEndPointId(turnPoint))
    return true;

  auto const it = m_noUTurnRestrictions.find(featureId);
  if (it == m_noUTurnRestrictions.cend())
    return false;

  auto const & uTurn = it->second;
  if (uTurn.m_atTheBegin && turnPoint == 0)
    return true;

  uint32_t const n = roadGeometry.GetPointsCount();
  ASSERT_GREATER_OR_EQUAL(n, 1, ());
  return uTurn.m_atTheEnd && turnPoint == n - 1;
}

RouteWeight IndexGraph::CalculateEdgeWeight(EdgeEstimator::Purpose purpose, bool isOutgoing,
                                            Segment const & from, Segment const & to,
                                            std::optional<RouteWeight const> const & prevWeight) const
{
  auto const & segment = isOutgoing ? to : from;
  auto const & road = GetRoadGeometry(segment.GetFeatureId());

  auto const weight = RouteWeight(m_estimator->CalcSegmentWeight(segment, road, purpose));
  auto const penalties = GetPenalties(purpose, isOutgoing ? from : to, isOutgoing ? to : from, prevWeight);

  return weight + penalties;
}
}  // namespace routing
