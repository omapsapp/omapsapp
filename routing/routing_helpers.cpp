#include "routing/routing_helpers.hpp"

#include "routing/directions_engine.hpp"
#include "routing/fake_feature_ids.hpp"
#include "routing/index_graph_starter.hpp"
#include "routing/route.hpp"
#include "routing/segment.hpp"
#include "routing/traffic_stash.hpp"
#include "routing/world_graph.hpp"

#include "geometry/point2d.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>
#include <utility>

namespace routing
{
using namespace std;
using namespace traffic;

void FillSegmentInfo(vector<double> const & times, vector<RouteSegment> & routeSegments)
{
  CHECK_EQUAL(times.size(), routeSegments.size(), ());
  ASSERT(is_sorted(times.cbegin(), times.cend()), ());

  if (routeSegments.empty())
    return;

  double routeLengthMeters = 0.0;
  double routeLengthMerc = 0.0;
  for (size_t i = 0; i < routeSegments.size(); ++i)
  {
    if (i > 0)
    {
      auto const & junction = routeSegments[i].GetJunction();
      auto const & prevJunction = routeSegments[i - 1].GetJunction();
      routeLengthMeters += mercator::DistanceOnEarth(junction.GetPoint(), prevJunction.GetPoint());
      routeLengthMerc += junction.GetPoint().Length(prevJunction.GetPoint());
    }

    routeSegments[i].SetDistancesAndTime(routeLengthMeters, routeLengthMerc, times[i]);
  }
}

void ReconstructRoute(DirectionsEngine & engine, IndexRoadGraph const & graph,
                      base::Cancellable const & cancellable,
                      vector<geometry::PointWithAltitude> const & path, vector<double> const & times,
                      Route & route)
{
  if (path.empty())
  {
    LOG(LERROR, ("Can't reconstruct route from an empty list of positions."));
    return;
  }

  CHECK_EQUAL(path.size(), times.size() + 1, ());

  vector<RouteSegment> routeSegments;
  if (!engine.Generate(graph, path, cancellable, routeSegments))
    return;

  if (cancellable.IsCancelled())
    return;

  FillSegmentInfo(times, routeSegments);
  route.SetRouteSegments(std::move(routeSegments));

  vector<m2::PointD> routeGeometry;
  JunctionsToPoints(path, routeGeometry);

  route.SetGeometry(routeGeometry.begin(), routeGeometry.end());

  LOG(LDEBUG, (route.DebugPrintTurns()));
}

Segment ConvertEdgeToSegment(NumMwmIds const & numMwmIds, Edge const & edge)
{
  if (edge.IsFake())
  {
    if (edge.HasRealPart())
    {
      return Segment(kFakeNumMwmId, FakeFeatureIds::kIndexGraphStarterId, edge.GetFakeSegmentId(),
                     true /* forward */);
    }

    return Segment();
  }

  auto const & fID = edge.GetFeatureId();
  NumMwmId const numMwmId = numMwmIds.GetId(fID.m_mwmId.GetInfo()->GetLocalFile().GetCountryFile());

  return Segment(numMwmId, fID.m_index, edge.GetSegId(), edge.IsForward());
}

bool SegmentCrossesRect(m2::Segment2D const & segment, m2::RectD const & rect)
{
  double constexpr kEps = 1e-6;
  bool isSideIntersected = false;
  rect.ForEachSide([&segment, &isSideIntersected](m2::PointD const & a, m2::PointD const & b) {
    if (isSideIntersected)
      return;

    m2::Segment2D const rectSide(a, b);
    isSideIntersected =
        m2::Intersect(segment, rectSide, kEps).m_type != m2::IntersectionResult::Type::Zero;
  });

  return isSideIntersected;
}

bool RectCoversPolyline(IRoadGraph::PointWithAltitudeVec const & junctions, m2::RectD const & rect)
{
  if (junctions.empty())
    return false;

  if (junctions.size() == 1)
    return rect.IsPointInside(junctions.front().GetPoint());

  for (auto const & junction : junctions)
  {
    if (rect.IsPointInside(junction.GetPoint()))
      return true;
  }

  // No point of polyline |junctions| lays inside |rect| but may be segments of the polyline
  // cross |rect| borders.
  for (size_t i = 1; i < junctions.size(); ++i)
  {
    m2::Segment2D const polylineSegment(junctions[i - 1].GetPoint(), junctions[i].GetPoint());
    if (SegmentCrossesRect(polylineSegment, rect))
      return true;
  }

  return false;
}

bool CheckGraphConnectivity(Segment const & start, bool isOutgoing, bool useRoutingOptions,
                            size_t limit, WorldGraph & graph, set<Segment> & marked)
{
  queue<Segment> q;
  q.push(start);

  marked.insert(start);

  WorldGraph::SegmentEdgeListT edges;
  while (!q.empty() && marked.size() < limit)
  {
    auto const u = q.front();
    q.pop();

    edges.clear();

    // Note. If |isOutgoing| == true outgoing edges are looked for.
    // If |isOutgoing| == false it's the finish. So ingoing edges are looked for.
    graph.GetEdgeList(u, isOutgoing, useRoutingOptions, edges);
    for (auto const & edge : edges)
    {
      auto const & v = edge.GetTarget();
      if (marked.count(v) == 0)
      {
        q.push(v);
        marked.insert(v);
      }
    }
  }

  return marked.size() >= limit;
}

// AStarLengthChecker ------------------------------------------------------------------------------

AStarLengthChecker::AStarLengthChecker(IndexGraphStarter & starter) : m_starter(starter) {}

bool AStarLengthChecker::operator()(RouteWeight const & weight) const
{
  return m_starter.CheckLength(weight);
}

// AdjustLengthChecker -----------------------------------------------------------------------------

AdjustLengthChecker::AdjustLengthChecker(IndexGraphStarter & starter) : m_starter(starter) {}

bool AdjustLengthChecker::operator()(RouteWeight const & weight) const
{
  // Limit of adjust in seconds.
  double constexpr kAdjustLimitSec = 5 * 60;
  return weight <= RouteWeight(kAdjustLimitSec) && m_starter.CheckLength(weight);
}
}  // namespace routing
