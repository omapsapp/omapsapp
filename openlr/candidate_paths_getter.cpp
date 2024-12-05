#include "openlr/candidate_paths_getter.hpp"

#include "openlr/candidate_points_getter.hpp"
#include "openlr/graph.hpp"
#include "openlr/helpers.hpp"
#include "openlr/openlr_model.hpp"

#include "routing/road_graph.hpp"

#include "platform/location.hpp"

#include "geometry/angles.hpp"
#include "geometry/point_with_altitude.hpp"

#include <algorithm>
#include <iterator>
#include <queue>
#include <set>
#include <tuple>

using namespace std;
using namespace routing;

namespace openlr
{
namespace cpg
{
int const kNumBuckets = 256;
double const kAnglesInBucket = 360.0 / kNumBuckets;

uint32_t Bearing(m2::PointD const & a, m2::PointD const & b)
{
  auto const angle = location::AngleToBearing(math::RadToDeg(ang::AngleTo(a, b)));
  CHECK_LESS_OR_EQUAL(angle, 360, ("Angle should be less than or equal to 360."));
  CHECK_GREATER_OR_EQUAL(angle, 0, ("Angle should be greater than or equal to 0"));
  return math::Clamp(angle / kAnglesInBucket, 0.0, 255.0);
}
}  // namespace cpg

// CandidatePathsGetter::Link ----------------------------------------------------------------------
Graph::Edge CandidatePathsGetter::Link::GetStartEdge() const
{
  auto * start = this;
  while (start->m_parent)
    start = start->m_parent.get();

  return start->m_edge;
}

bool CandidatePathsGetter::Link::IsPointOnPath(geometry::PointWithAltitude const & point) const
{
  for (auto * l = this; l; l = l->m_parent.get())
  {
    if (l->m_edge.GetEndJunction() == point)
    {
      LOG(LDEBUG, ("A loop detected, skipping..."));
      return true;
    }
  }

  return false;
}

// CandidatePathsGetter ----------------------------------------------------------------------------
bool CandidatePathsGetter::GetLineCandidatesForPoints(
    vector<LocationReferencePoint> const & points,
    vector<vector<Graph::EdgeVector>> & lineCandidates)
{
  for (size_t i = 0; i < points.size(); ++i)
  {
    if (i != points.size() - 1 && points[i].m_distanceToNextPoint == 0)
    {
      LOG(LINFO, ("Distance to next point is zero. Skipping the whole segment"));
      ++m_stats.m_zeroDistToNextPointCount;
      return false;
    }

    lineCandidates.emplace_back();
    auto const isLastPoint = i == points.size() - 1;
    double const distanceToNextPointM =
        (isLastPoint ? points[i - 1] : points[i]).m_distanceToNextPoint;

    vector<m2::PointD> pointCandidates;
    m_pointsGetter.GetCandidatePoints(mercator::FromLatLon(points[i].m_latLon),
                                      pointCandidates);
    GetLineCandidates(points[i], isLastPoint, distanceToNextPointM, pointCandidates,
                      lineCandidates.back());

    if (lineCandidates.back().empty())
    {
      LOG(LINFO, ("No candidate lines found for point", points[i].m_latLon, "Giving up"));
      ++m_stats.m_noCandidateFound;
      return false;
    }
  }

  ASSERT_EQUAL(lineCandidates.size(), points.size(), ());

  return true;
}

void CandidatePathsGetter::GetStartLines(vector<m2::PointD> const & points, bool const isLastPoint,
                                         Graph::EdgeVector & edges)
{
  for (auto const & pc : points)
  {
    Graph::EdgeListT tmp;
    if (!isLastPoint)
      m_graph.GetOutgoingEdges(geometry::PointWithAltitude(pc, 0 /* altitude */), tmp);
    else
      m_graph.GetIngoingEdges(geometry::PointWithAltitude(pc, 0 /* altitude */), tmp);

    edges.insert(edges.end(), tmp.begin(), tmp.end());
  }

  // Same edges may start on different points if those points are close enough.
  base::SortUnique(edges, less<Graph::Edge>(), EdgesAreAlmostEqual);
}

void CandidatePathsGetter::GetAllSuitablePaths(Graph::EdgeVector const & startLines,
                                               bool isLastPoint, double bearDistM,
                                               FunctionalRoadClass functionalRoadClass,
                                               FormOfWay formOfWay, double distanceToNextPointM,
                                               vector<LinkPtr> & allPaths)
{
  queue<LinkPtr> q;

  for (auto const & e : startLines)
  {
    auto const u = make_shared<Link>(nullptr /* parent */, e, 0 /* distanceM */);
    q.push(u);
  }

  while (!q.empty())
  {
    auto const u = q.front();
    q.pop();

    auto const & currentEdge = u->m_edge;
    auto const currentEdgeLen = EdgeLength(currentEdge);

    // TODO(mgsergio): Maybe weak this constraint a bit.
    if (u->m_distanceM + currentEdgeLen >= bearDistM)
    {
      allPaths.push_back(u);
      continue;
    }

    ASSERT_LESS(u->m_distanceM + currentEdgeLen, bearDistM, ());

    Graph::EdgeListT edges;
    if (!isLastPoint)
      m_graph.GetOutgoingEdges(currentEdge.GetEndJunction(), edges);
    else
      m_graph.GetIngoingEdges(currentEdge.GetStartJunction(), edges);

    for (auto const & e : edges)
    {
      // Fake edges are allowed only at the start/end of the path.
      if (e.IsFake())
        continue;

      if (EdgesAreAlmostEqual(e.GetReverseEdge(), currentEdge))
        continue;

      ASSERT(currentEdge.HasRealPart(), ());

      if (!PassesRestriction(e, functionalRoadClass, formOfWay, 2 /* kFRCThreshold */, m_infoGetter))
        continue;

      // TODO(mgsergio): Should we check form of way as well?

      if (u->IsPointOnPath(e.GetEndJunction()))
        continue;

      auto const p = make_shared<Link>(u, e, u->m_distanceM + currentEdgeLen);
      q.push(p);
    }
  }
}

void CandidatePathsGetter::GetBestCandidatePaths(
    vector<LinkPtr> const & allPaths, bool const isLastPoint, uint32_t const requiredBearing,
    double const bearDistM, m2::PointD const & startPoint, vector<Graph::EdgeVector> & candidates)
{
  set<CandidatePath> candidatePaths;
  set<CandidatePath> fakeEndingsCandidatePaths;

  BearingPointsSelector pointsSelector(bearDistM, isLastPoint);
  for (auto const & l : allPaths)
  {
    auto const bearStartPoint = pointsSelector.GetStartPoint(l->GetStartEdge());
    auto const startPointsDistance = mercator::DistanceOnEarth(bearStartPoint, startPoint);

    // Number of edges counting from the last one to check bearing on. Accorfing to OpenLR spec
    // we have to check bearing on a point that is no longer than 25 meters traveling down the path.
    // But sometimes we may skip the best place to stop and generate a candidate. So we check several
    // edges before the last one to avoid such a situation. Number of iterations is taken
    // by intuition.
    // Example:
    // o -------- o  { Partners segment. }
    // o ------- o --- o { Our candidate. }
    //               ^ 25m
    //           ^ This one may be better than
    //                 ^ this one.
    // So we want to check them all.
    uint32_t traceBackIterationsLeft = 3;
    for (auto part = l; part; part = part->m_parent)
    {
      if (traceBackIterationsLeft == 0)
        break;

      --traceBackIterationsLeft;

      auto const bearEndPoint =
          pointsSelector.GetEndPoint(part->m_edge, part->m_distanceM);

      auto const bearing = cpg::Bearing(bearStartPoint, bearEndPoint);
      auto const bearingDiff = AbsDifference(bearing, requiredBearing);
      auto const pathDistDiff = AbsDifference(part->m_distanceM, bearDistM);

      // TODO(mgsergio): Check bearing is within tolerance. Add to canidates if it is.

      if (part->m_hasFake)
        fakeEndingsCandidatePaths.emplace(part, bearingDiff, pathDistDiff, startPointsDistance);
      else
        candidatePaths.emplace(part, bearingDiff, pathDistDiff, startPointsDistance);
    }
  }

  ASSERT(
      none_of(begin(candidatePaths), end(candidatePaths), mem_fn(&CandidatePath::HasFakeEndings)),
      ());
  ASSERT(fakeEndingsCandidatePaths.empty() ||
             any_of(begin(fakeEndingsCandidatePaths), end(fakeEndingsCandidatePaths),
                    mem_fn(&CandidatePath::HasFakeEndings)),
         ());

  vector<CandidatePath> paths;
  copy_n(begin(candidatePaths), min(static_cast<size_t>(kMaxCandidates), candidatePaths.size()),
         back_inserter(paths));

  copy_n(begin(fakeEndingsCandidatePaths),
         min(static_cast<size_t>(kMaxFakeCandidates), fakeEndingsCandidatePaths.size()),
         back_inserter(paths));

  LOG(LDEBUG, ("List candidate paths..."));
  for (auto const & path : paths)
  {
    LOG(LDEBUG, ("CP:", path.m_bearingDiff, path.m_pathDistanceDiff, path.m_startPointDistance));
    Graph::EdgeVector edges;
    for (auto * p = path.m_path.get(); p; p = p->m_parent.get())
      edges.push_back(p->m_edge);
    if (!isLastPoint)
      reverse(begin(edges), end(edges));

    candidates.emplace_back(std::move(edges));
  }
}

void CandidatePathsGetter::GetLineCandidates(openlr::LocationReferencePoint const & p,
                                             bool const isLastPoint,
                                             double const distanceToNextPointM,
                                             vector<m2::PointD> const & pointCandidates,
                                             vector<Graph::EdgeVector> & candidates)
{
  double const kDefaultBearDistM = 25.0;
  double const bearDistM = min(kDefaultBearDistM, distanceToNextPointM);

  LOG(LINFO, ("BearDist is", bearDistM));

  Graph::EdgeVector startLines;
  GetStartLines(pointCandidates, isLastPoint, startLines);

  LOG(LINFO, (startLines.size(), "start line candidates found for point (LatLon)", p.m_latLon));
  LOG(LDEBUG, ("Listing start lines:"));
  for (auto const & e : startLines)
    LOG(LDEBUG, (LogAs2GisPath(e)));

  auto const startPoint = mercator::FromLatLon(p.m_latLon);

  vector<LinkPtr> allPaths;
  GetAllSuitablePaths(startLines, isLastPoint, bearDistM, p.m_functionalRoadClass, p.m_formOfWay,
                      distanceToNextPointM, allPaths);
  GetBestCandidatePaths(allPaths, isLastPoint, p.m_bearing, bearDistM, startPoint, candidates);
  LOG(LDEBUG, (candidates.size(), "candidate paths found for point (LatLon)", p.m_latLon));
}
}  // namespace openlr
