#include "openlr/score_candidate_paths_getter.hpp"

#include "openlr/graph.hpp"
#include "openlr/openlr_model.hpp"
#include "openlr/score_candidate_points_getter.hpp"

#include "routing/road_graph.hpp"

#include "platform/location.hpp"

#include "geometry/angles.hpp"
#include "geometry/mercator.hpp"
#include "geometry/point_with_altitude.hpp"

#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <queue>
#include <set>
#include <tuple>
#include <utility>

using namespace routing;
using namespace std;

namespace openlr
{
namespace scpg
{
int constexpr kNumBuckets = 256;
double constexpr kAnglesInBucket = 360.0 / kNumBuckets;

double ToAngleInDeg(uint32_t angleInBuckets)
{
  CHECK_LESS_OR_EQUAL(angleInBuckets, 255, ());
  return base::Clamp(kAnglesInBucket * static_cast<double>(angleInBuckets), 0.0, 360.0);
}

uint32_t BearingInDeg(m2::PointD const & a, m2::PointD const & b)
{
  auto const angle = location::AngleToBearing(base::RadToDeg(ang::AngleTo(a, b)));
  CHECK(0.0 <= angle && angle <= 360.0, (angle));
  return angle;
}

double DifferenceInDeg(double a1, double a2)
{
  auto const diff = 180.0 - abs(abs(a1 - a2) - 180.0);
  CHECK(0.0 <= diff && diff <= 180.0, (diff));
  return diff;
}

void EdgeSortUniqueByStartAndEndPoints(Graph::EdgeListT & edges)
{
  base::SortUnique(
      edges,
      [](Edge const & e1, Edge const & e2) {
        if (e1.GetStartPoint() != e2.GetStartPoint())
          return e1.GetStartPoint() < e2.GetStartPoint();
        return e1.GetEndPoint() < e2.GetEndPoint();
      },
      [](Edge const & e1, Edge const & e2) {
        return e1.GetStartPoint() == e2.GetStartPoint() && e1.GetEndPoint() == e2.GetEndPoint();
      });
}
}  // namespace scpg

// ScoreCandidatePathsGetter::Link ----------------------------------------------------------------------
Graph::Edge ScoreCandidatePathsGetter::Link::GetStartEdge() const
{
  auto * start = this;
  while (start->m_parent)
    start = start->m_parent.get();

  return start->m_edge;
}

bool ScoreCandidatePathsGetter::Link::IsJunctionInPath(geometry::PointWithAltitude const & j) const
{
  for (auto * l = this; l; l = l->m_parent.get())
  {
    if (l->m_edge.GetEndJunction().GetPoint() == j.GetPoint())
    {
      LOG(LDEBUG, ("A loop detected, skipping..."));
      return true;
    }
  }

  return false;
}

// ScoreCandidatePathsGetter ----------------------------------------------------------------------------
bool ScoreCandidatePathsGetter::GetLineCandidatesForPoints(
    vector<LocationReferencePoint> const & points, LinearSegmentSource source,
    vector<ScorePathVec> & lineCandidates)
{
  CHECK_GREATER(points.size(), 1, ());

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

    ScoreEdgeVec edgesCandidates;
    m_pointsGetter.GetEdgeCandidates(mercator::FromLatLon(points[i].m_latLon),
                                     isLastPoint, edgesCandidates);

    GetLineCandidates(points[i], source, isLastPoint, distanceToNextPointM, edgesCandidates,
                      lineCandidates.back());

    if (lineCandidates.back().empty())
    {
      LOG(LINFO, ("No candidate lines found for point", points[i].m_latLon, "Giving up"));
      ++m_stats.m_noCandidateFound;
      return false;
    }
  }

  CHECK_EQUAL(lineCandidates.size(), points.size(), ());
  return true;
}

void ScoreCandidatePathsGetter::GetAllSuitablePaths(ScoreEdgeVec const & startLines,
                                                    LinearSegmentSource source, bool isLastPoint,
                                                    double bearDistM,
                                                    FunctionalRoadClass functionalRoadClass,
                                                    FormOfWay formOfWay,
                                                    double distanceToNextPointM,
                                                    vector<shared_ptr<Link>> & allPaths)
{
  CHECK_NOT_EQUAL(source, LinearSegmentSource::NotValid, ());

  queue<shared_ptr<Link>> q;

  for (auto const & e : startLines)
  {
    Score roadScore = 0; // Score based on functional road class and form of way.
    if (source == LinearSegmentSource::FromLocationReferenceTag &&
        !PassesRestrictionV3(e.m_edge, functionalRoadClass, formOfWay, m_infoGetter, roadScore))
    {
      continue;
    }

    q.push(
        make_shared<Link>(nullptr /* parent */, e.m_edge, 0 /* distanceM */, e.m_score, roadScore));
  }

  // Filling |allPaths| staring from |startLines| which have passed functional road class
  // and form of way restrictions. All paths in |allPaths| are shorter then |bearDistM| plus
  // one segment length.
  while (!q.empty())
  {
    auto const u = q.front();
    q.pop();

    auto const & currentEdge = u->m_edge;
    auto const currentEdgeLen = EdgeLength(currentEdge);

    // The path from the start of the openlr segment to the finish to the openlr segment should be
    // much shorter then the distance of any connection of openlr segment.
    // This condition should be checked because otherwise in rare case |q| may be overfilled.
    if (u->m_distanceM > distanceToNextPointM)
      continue;

    if (u->m_distanceM + currentEdgeLen >= bearDistM)
    {
      allPaths.emplace_back(std::move(u));
      continue;
    }

    CHECK_LESS(u->m_distanceM + currentEdgeLen, bearDistM, ());

    Graph::EdgeListT edges;
    if (!isLastPoint)
      m_graph.GetOutgoingEdges(currentEdge.GetEndJunction(), edges);
    else
      m_graph.GetIngoingEdges(currentEdge.GetStartJunction(), edges);

    // It's possible that road edges are duplicated a lot of times. It's a error but
    // a mapper may do that. To prevent a combinatorial explosion while matching
    // duplicated edges should be removed.
    scpg::EdgeSortUniqueByStartAndEndPoints(edges);

    for (auto const & e : edges)
    {
      CHECK(!e.IsFake(), ());

      if (EdgesAreAlmostEqual(e.GetReverseEdge(), currentEdge))
        continue;

      CHECK(currentEdge.HasRealPart(), ());

      Score roadScore = 0;
      if (source == LinearSegmentSource::FromLocationReferenceTag &&
          !PassesRestrictionV3(e, functionalRoadClass, formOfWay, m_infoGetter, roadScore))
      {
        continue;
      }

      if (u->IsJunctionInPath(e.GetEndJunction()))
        continue;

      // Road score for a path is minimum value of score of segments based on functional road class
      // of the segments and form of way of the segments.
      q.emplace(make_shared<Link>(u, e, u->m_distanceM + currentEdgeLen, u->m_pointScore,
                                  min(roadScore, u->m_minRoadScore)));
    }
  }
}

void ScoreCandidatePathsGetter::GetBestCandidatePaths(vector<shared_ptr<Link>> const & allPaths,
                                                      LinearSegmentSource source, bool isLastPoint,
                                                      uint32_t requiredBearing, double bearDistM, m2::PointD const &,
                                                      ScorePathVec & candidates)
{
  CHECK_NOT_EQUAL(source, LinearSegmentSource::NotValid, ());
  CHECK_LESS_OR_EQUAL(requiredBearing, 255, ());

  multiset<CandidatePath, greater<>> candidatePaths;

  BearingPointsSelector pointsSelector(static_cast<uint32_t>(bearDistM), isLastPoint);
  for (auto const & link : allPaths)
  {
    auto const bearStartPoint = pointsSelector.GetStartPoint(link->GetStartEdge());

    // Number of edges counting from the last one to check bearing on. According to OpenLR spec
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
    for (auto part = link; part; part = part->m_parent)
    {
      if (traceBackIterationsLeft == 0)
        break;

      --traceBackIterationsLeft;

      // Note. No information about bearing if source == LinearSegmentSource::FromCoordinatesTag.
      Score bearingScore = 0;
      if (source == LinearSegmentSource::FromLocationReferenceTag)
      {
        if (!GetBearingScore(pointsSelector, *part, bearStartPoint, requiredBearing, bearingScore))
          continue;
      }
      candidatePaths.emplace(part, part->m_pointScore, part->m_minRoadScore, bearingScore);
    }
  }

  size_t constexpr kMaxCandidates = 7;
  vector<CandidatePath> paths;
  copy_n(candidatePaths.begin(), min(static_cast<size_t>(kMaxCandidates), candidatePaths.size()),
         back_inserter(paths));

  for (auto const & path : paths)
  {
    Graph::EdgeVector edges;
    for (auto * p = path.m_path.get(); p; p = p->m_parent.get())
      edges.push_back(p->m_edge);

    if (!isLastPoint)
      reverse(edges.begin(), edges.end());

    candidates.emplace_back(path.GetScore(), std::move(edges));
  }
}

void ScoreCandidatePathsGetter::GetLineCandidates(openlr::LocationReferencePoint const & p,
                                                  LinearSegmentSource source,
                                                  bool isLastPoint,
                                                  double distanceToNextPointM,
                                                  ScoreEdgeVec const & edgeCandidates,
                                                  ScorePathVec & candidates)
{
  double constexpr kDefaultBearDistM = 25.0;
  double const bearDistM = min(kDefaultBearDistM, distanceToNextPointM);

  ScoreEdgeVec const & startLines = edgeCandidates;
  LOG(LDEBUG, ("Listing start lines:"));
  for (auto const & e : startLines)
    LOG(LDEBUG, (LogAs2GisPath(e.m_edge)));

  auto const startPoint = mercator::FromLatLon(p.m_latLon);

  vector<shared_ptr<Link>> allPaths;
  GetAllSuitablePaths(startLines, source, isLastPoint, bearDistM, p.m_functionalRoadClass,
                      p.m_formOfWay, distanceToNextPointM, allPaths);

  GetBestCandidatePaths(allPaths, source, isLastPoint, p.m_bearing, bearDistM, startPoint,
                        candidates);
  // Sorting by increasing order.
  sort(candidates.begin(), candidates.end(),
       [](ScorePath const & s1, ScorePath const & s2) { return s1.m_score > s2.m_score; });
  LOG(LDEBUG, (candidates.size(), "Candidate paths found for point:", p.m_latLon));
}

bool ScoreCandidatePathsGetter::GetBearingScore(BearingPointsSelector const & pointsSelector,
                                                ScoreCandidatePathsGetter::Link const & part,
                                                m2::PointD const & bearStartPoint,
                                                uint32_t requiredBearing, Score & score)
{
  auto const bearEndPoint = pointsSelector.GetEndPoint(part.m_edge, part.m_distanceM);

  auto const bearingDeg = scpg::BearingInDeg(bearStartPoint, bearEndPoint);
  double const requiredBearingDeg = scpg::ToAngleInDeg(requiredBearing);
  double const angleDeviationDeg = scpg::DifferenceInDeg(bearingDeg, requiredBearingDeg);

  // If the bearing according to osm segments (|bearingDeg|) is significantly different
  // from the bearing set in openlr (|requiredBearingDeg|) the candidate should be skipped.
  double constexpr kMinAngleDeviationDeg = 50.0;
  if (angleDeviationDeg > kMinAngleDeviationDeg)
    return false;

  double constexpr kMaxScoreForBearing = 60.0;
  double constexpr kAngleDeviationFactor = 1.0 / 4.3;
  score =
      static_cast<Score>(kMaxScoreForBearing / (1.0 + angleDeviationDeg * kAngleDeviationFactor));

  return true;
}
}  // namespace openlr
