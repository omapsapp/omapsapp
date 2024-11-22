#include "openlr/router.hpp"

#include "openlr/helpers.hpp"
#include "openlr/road_info_getter.hpp"

#include "routing/features_road_graph.hpp"

#include "platform/location.hpp"

#include "geometry/angles.hpp"
#include "geometry/mercator.hpp"
#include "geometry/segment2d.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

#include <boost/iterator/transform_iterator.hpp>

using boost::make_transform_iterator;

namespace openlr
{
namespace
{
int const kFRCThreshold = 2;
size_t const kMaxRoadCandidates = 20;
double const kDistanceAccuracyM = 1000;
double const kEps = 1e-9;
double const kBearingDist = 25;

int const kNumBuckets = 256;
double const kAnglesInBucket = 360.0 / kNumBuckets;

uint32_t Bearing(m2::PointD const & a, m2::PointD const & b)
{
  auto const angle = location::AngleToBearing(math::RadToDeg(ang::AngleTo(a, b)));
  CHECK_LESS_OR_EQUAL(angle, 360, ("Angle should be less than or equal to 360."));
  CHECK_GREATER_OR_EQUAL(angle, 0, ("Angle should be greater than or equal to 0"));
  return math::Clamp(angle / kAnglesInBucket, 0.0, 255.0);
}
}  // namespace

// Router::Vertex::Score ---------------------------------------------------------------------------
void Router::Vertex::Score::AddFakePenalty(double p, bool partOfReal)
{
  m_penalty += (partOfReal ? kFakeCoeff : kTrueFakeCoeff) * p;
}

void Router::Vertex::Score::AddBearingPenalty(int expected, int actual)
{
  ASSERT_LESS(expected, kNumBuckets, ());
  ASSERT_GREATER_OR_EQUAL(expected, 0, ());

  ASSERT_LESS(actual, kNumBuckets, ());
  ASSERT_GREATER_OR_EQUAL(actual, 0, ());

  int const diff = abs(expected - actual);
  double angle = math::DegToRad(std::min(diff, kNumBuckets - diff) * kAnglesInBucket);
  m_penalty += kBearingErrorCoeff * angle * kBearingDist;
}

bool Router::Vertex::Score::operator<(Score const & rhs) const
{
  auto const ls = GetScore();
  auto const rs = rhs.GetScore();
  if (ls != rs)
    return ls < rs;

  if (m_distance != rhs.m_distance)
    return m_distance < rhs.m_distance;
  return m_penalty < rhs.m_penalty;
}

bool Router::Vertex::Score::operator==(Score const & rhs) const
{
  return m_distance == rhs.m_distance && m_penalty == rhs.m_penalty;
}

// Router::Vertex ----------------------------------------------------------------------------------
Router::Vertex::Vertex(geometry::PointWithAltitude const & junction,
                       geometry::PointWithAltitude const & stageStart, double stageStartDistance,
                       size_t stage, bool bearingChecked)
  : m_junction(junction)
  , m_stageStart(stageStart)
  , m_stageStartDistance(stageStartDistance)
  , m_stage(stage)
  , m_bearingChecked(bearingChecked)
{
}

bool Router::Vertex::operator<(Vertex const & rhs) const
{
  if (m_junction != rhs.m_junction)
    return m_junction < rhs.m_junction;
  if (m_stageStart != rhs.m_stageStart)
    return m_stageStart < rhs.m_stageStart;
  if (m_stageStartDistance != rhs.m_stageStartDistance)
    return m_stageStartDistance < rhs.m_stageStartDistance;
  if (m_stage != rhs.m_stage)
    return m_stage < rhs.m_stage;
  return m_bearingChecked < rhs.m_bearingChecked;
}

bool Router::Vertex::operator==(Vertex const & rhs) const
{
  return m_junction == rhs.m_junction && m_stageStart == rhs.m_stageStart &&
         m_stageStartDistance == rhs.m_stageStartDistance && m_stage == rhs.m_stage &&
         m_bearingChecked == rhs.m_bearingChecked;
}

// Router::Edge ------------------------------------------------------------------------------------
Router::Edge::Edge(Vertex const & u, Vertex const & v, routing::Edge const & raw, bool isSpecial)
  : m_u(u), m_v(v), m_raw(raw), m_isSpecial(isSpecial)
{
}

// static
Router::Edge Router::Edge::MakeNormal(Vertex const & u, Vertex const & v, routing::Edge const & raw)
{
  return Edge(u, v, raw, false /* isSpecial */);
}

// static
Router::Edge Router::Edge::MakeSpecial(Vertex const & u, Vertex const & v)
{
  return Edge(u, v, routing::Edge::MakeFake(u.m_junction, v.m_junction), true /* isSpecial */);
}

std::pair<m2::PointD, m2::PointD> Router::Edge::ToPair() const
{
  auto const & e = m_raw;
  return std::make_pair(e.GetStartJunction().GetPoint(), e.GetEndJunction().GetPoint());
}

std::pair<m2::PointD, m2::PointD> Router::Edge::ToPairRev() const
{
  auto const & e = m_raw;
  return std::make_pair(e.GetEndJunction().GetPoint(), e.GetStartJunction().GetPoint());
}

// Router::Router ----------------------------------------------------------------------------------
Router::Router(routing::FeaturesRoadGraph & graph, RoadInfoGetter & roadInfoGetter)
  : m_graph(graph), m_roadInfoGetter(roadInfoGetter)
{
}

bool Router::Go(std::vector<WayPoint> const & points, double positiveOffsetM,
                double negativeOffsetM, Path & path)
{
  if (!Init(points, positiveOffsetM, negativeOffsetM))
    return false;
  return FindPath(path);
}

bool Router::Init(std::vector<WayPoint> const & points, double positiveOffsetM,
                  double negativeOffsetM)
{
  CHECK_GREATER_OR_EQUAL(points.size(), 2, ());

  m_points = points;
  m_positiveOffsetM = positiveOffsetM;
  m_negativeOffsetM = negativeOffsetM;

  m_graph.ResetFakes();

  m_pivots.clear();
  for (size_t i = 1; i + 1 < m_points.size(); ++i)
  {
    m_pivots.emplace_back();
    auto & ps = m_pivots.back();

    std::vector<std::pair<routing::Edge, geometry::PointWithAltitude>> vicinity;
    m_graph.FindClosestEdges(
        mercator::RectByCenterXYAndSizeInMeters(m_points[i].m_point,
                                                routing::FeaturesRoadGraph::kClosestEdgesRadiusM),
        kMaxRoadCandidates, vicinity);
    for (auto const & v : vicinity)
    {
      auto const & e = v.first;
      ps.push_back(e.GetStartJunction().GetPoint());
      ps.push_back(e.GetEndJunction().GetPoint());
    }

    if (ps.empty())
      return false;
  }

  m_pivots.push_back({m_points.back().m_point});
  CHECK_EQUAL(m_pivots.size() + 1, m_points.size(), ());

  {
    m_sourceJunction = geometry::PointWithAltitude(m_points.front().m_point, 0 /* altitude */);
    std::vector<std::pair<routing::Edge, geometry::PointWithAltitude>> sourceVicinity;
    m_graph.FindClosestEdges(
        mercator::RectByCenterXYAndSizeInMeters(m_sourceJunction.GetPoint(),
                                                routing::FeaturesRoadGraph::kClosestEdgesRadiusM),
        kMaxRoadCandidates, sourceVicinity);
    m_graph.AddFakeEdges(m_sourceJunction, sourceVicinity);
  }

  {
    m_targetJunction = geometry::PointWithAltitude(m_points.back().m_point, 0 /* altitude */);
    std::vector<std::pair<routing::Edge, geometry::PointWithAltitude>> targetVicinity;
    m_graph.FindClosestEdges(
        mercator::RectByCenterXYAndSizeInMeters(m_targetJunction.GetPoint(),
                                                routing::FeaturesRoadGraph::kClosestEdgesRadiusM),
        kMaxRoadCandidates, targetVicinity);
    m_graph.AddFakeEdges(m_targetJunction, targetVicinity);
  }

  return true;
}

bool Router::FindPath(Path & path)
{
  using State = std::pair<Vertex::Score, Vertex>;
  std::priority_queue<State, std::vector<State>, std::greater<State>> queue;
  std::map<Vertex, Vertex::Score> scores;
  Links links;

  auto const pushVertex = [&queue, &scores, &links](Vertex const & u, Vertex const & v,
                                                    Vertex::Score const & vertexScore,
                                                    Edge const & e) {
    if ((scores.count(v) == 0 || scores[v].GetScore() > vertexScore.GetScore() + kEps) && u != v)
    {
      scores[v] = vertexScore;
      links[v] = std::make_pair(u, e);
      queue.emplace(vertexScore, v);
    }
  };

  Vertex const s(m_sourceJunction, m_sourceJunction, 0 /* stageStartDistance */, 0 /* stage */,
                 false /* bearingChecked */);
  CHECK(!NeedToCheckBearing(s, 0 /* distance */), ());

  scores[s] = Vertex::Score();
  queue.emplace(scores[s], s);

  double const piS = GetPotential(s);

  while (!queue.empty())
  {
    auto const p = queue.top();
    queue.pop();

    Vertex::Score const & su = p.first;
    Vertex const & u = p.second;

    if (su != scores[u])
      continue;

    if (IsFinalVertex(u))
    {
      std::vector<Edge> edges;
      auto cur = u;
      while (cur != s)
      {
        auto const & p = links[cur];

        edges.push_back(p.second);
        cur = p.first;
      }
      reverse(edges.begin(), edges.end());
      return ReconstructPath(edges, path);
    }

    size_t const stage = u.m_stage;
    double const distanceToNextPointM = m_points[stage].m_distanceToNextPointM;
    double const piU = GetPotential(u);
    double const ud = su.GetDistance() + piS - piU;  // real distance to u

    CHECK_LESS(stage, m_pivots.size(), ());

    // max(kDistanceAccuracyM, m_distanceToNextPointM) is added here
    // to throw out quite long paths.
    if (ud > u.m_stageStartDistance + distanceToNextPointM +
                 std::max(kDistanceAccuracyM, distanceToNextPointM))
    {
      continue;
    }

    if (NearNextStage(u, piU) && !u.m_bearingChecked)
    {
      Vertex v = u;

      Vertex::Score sv = su;
      if (u.m_junction != u.m_stageStart)
      {
        int const expected = m_points[stage].m_bearing;
        int const actual = Bearing(u.m_stageStart.GetPoint(), u.m_junction.GetPoint());
        sv.AddBearingPenalty(expected, actual);
      }
      v.m_bearingChecked = true;

      pushVertex(u, v, sv, Edge::MakeSpecial(u, v));
    }

    if (MayMoveToNextStage(u, piU))
    {
      Vertex v(u.m_junction, u.m_junction, ud /* stageStartDistance */, stage + 1,
               false /* bearingChecked */);
      double const piV = GetPotential(v);

      Vertex::Score sv = su;
      sv.AddDistance(std::max(piV - piU, 0.0));
      sv.AddIntermediateErrorPenalty(
          mercator::DistanceOnEarth(v.m_junction.GetPoint(), m_points[v.m_stage].m_point));

      if (IsFinalVertex(v))
      {
        int const expected = m_points.back().m_bearing;
        int const actual = GetReverseBearing(u, links);
        sv.AddBearingPenalty(expected, actual);
      }

      pushVertex(u, v, sv, Edge::MakeSpecial(u, v));
    }

    ForEachEdge(u, true /* outgoing */, m_points[stage].m_lfrcnp, [&](routing::Edge const & edge) {
      Vertex v = u;
      v.m_junction = edge.GetEndJunction();

      double const piV = GetPotential(v);

      Vertex::Score sv = su;
      double const w = GetWeight(edge);
      sv.AddDistance(std::max(w + piV - piU, 0.0));

      double const vd = ud + w;  // real distance to v
      if (NeedToCheckBearing(v, vd))
      {
        CHECK(!NeedToCheckBearing(u, ud), ());

        double const delta = vd - v.m_stageStartDistance - kBearingDist;
        auto const p = PointAtSegment(edge.GetStartJunction().GetPoint(),
                                      edge.GetEndJunction().GetPoint(), delta);
        if (v.m_stageStart.GetPoint() != p)
        {
          int const expected = m_points[stage].m_bearing;
          int const actual = Bearing(v.m_stageStart.GetPoint(), p);
          sv.AddBearingPenalty(expected, actual);
        }
        v.m_bearingChecked = true;
      }

      if (vd > v.m_stageStartDistance + distanceToNextPointM)
        sv.AddDistanceErrorPenalty(std::min(vd - v.m_stageStartDistance - distanceToNextPointM, w));

      if (edge.IsFake())
        sv.AddFakePenalty(w, edge.HasRealPart());

      pushVertex(u, v, sv, Edge::MakeNormal(u, v, edge));
    });
  }

  return false;
}

bool Router::NeedToCheckBearing(Vertex const & u, double distanceM) const
{
  if (IsFinalVertex(u) || u.m_bearingChecked)
    return false;
  return distanceM >= u.m_stageStartDistance + kBearingDist;
}

double Router::GetPotential(Vertex const & u) const
{
  if (IsFinalVertex(u))
    return 0.0;

  CHECK_LESS(u.m_stage, m_pivots.size(), ());

  auto const & pivots = m_pivots[u.m_stage];
  CHECK(!pivots.empty(), ("Empty list of pivots"));

  double potential = std::numeric_limits<double>::max();

  auto const & point = u.m_junction.GetPoint();
  for (auto const & pivot : pivots)
    potential = std::min(potential, mercator::DistanceOnEarth(pivot, point));
  return potential;
}

bool Router::NearNextStage(Vertex const & u, double pi) const
{
  return u.m_stage < m_pivots.size() && pi < kEps;
}

bool Router::MayMoveToNextStage(Vertex const & u, double pi) const
{
  return NearNextStage(u, pi) && u.m_bearingChecked;
}

uint32_t Router::GetReverseBearing(Vertex const & u, Links const & links) const
{
  m2::PointD const a = u.m_junction.GetPoint();
  m2::PointD b = m2::PointD::Zero();

  Vertex curr = u;
  double passed = 0;
  bool found = false;
  while (true)
  {
    auto const it = links.find(curr);
    if (it == links.end())
      break;

    auto const & p = it->second;
    auto const & prev = p.first;
    auto const & edge = p.second.m_raw;

    if (prev.m_stage != curr.m_stage)
      break;

    double const weight = GetWeight(edge);

    if (passed + weight >= kBearingDist)
    {
      double const delta = kBearingDist - passed;
      b = PointAtSegment(edge.GetEndJunction().GetPoint(), edge.GetStartJunction().GetPoint(),
                         delta);
      found = true;
      break;
    }

    passed += weight;
    curr = prev;
  }
  if (!found)
    b = curr.m_junction.GetPoint();
  return Bearing(a, b);
}

template <typename Fn>
void Router::ForEachEdge(Vertex const & u, bool outgoing, FunctionalRoadClass restriction, Fn && fn)
{
  routing::IRoadGraph::EdgeListT edges;
  if (outgoing)
    GetOutgoingEdges(u.m_junction, edges);
  else
    GetIngoingEdges(u.m_junction, edges);
  for (auto const & edge : edges)
  {
    if (!ConformLfrcnp(edge, restriction, kFRCThreshold, m_roadInfoGetter))
      continue;
    fn(edge);
  }
}

void Router::GetOutgoingEdges(geometry::PointWithAltitude const & u,
                              routing::IRoadGraph::EdgeListT & edges)
{
  GetEdges(u, &routing::IRoadGraph::GetRegularOutgoingEdges,
           &routing::IRoadGraph::GetFakeOutgoingEdges, m_outgoingCache, edges);
}

void Router::GetIngoingEdges(geometry::PointWithAltitude const & u,
                             routing::IRoadGraph::EdgeListT & edges)
{
  GetEdges(u, &routing::IRoadGraph::GetRegularIngoingEdges,
           &routing::IRoadGraph::GetFakeIngoingEdges, m_ingoingCache, edges);
}

void Router::GetEdges(
    geometry::PointWithAltitude const & u, RoadGraphEdgesGetter getRegular,
    RoadGraphEdgesGetter getFake,
    EdgeCacheT & cache,
    routing::IRoadGraph::EdgeListT & edges)
{
  auto const it = cache.find(u);
  if (it == cache.end())
  {
    auto & es = cache[u];
    (m_graph.*getRegular)(u, es);
    edges.append(es.begin(), es.end());
  }
  else
  {
    auto const & es = it->second;
    edges.append(es.begin(), es.end());
  }
  (m_graph.*getFake)(u, edges);
}

template <typename Fn>
void Router::ForEachNonFakeEdge(Vertex const & u, bool outgoing, FunctionalRoadClass restriction,
                                Fn && fn)
{
  ForEachEdge(u, outgoing, restriction, [&fn](routing::Edge const & edge) {
    if (!edge.IsFake())
      fn(edge);
  });
}

template <typename Fn>
void Router::ForEachNonFakeClosestEdge(Vertex const & u, FunctionalRoadClass const restriction,
                                       Fn && fn)
{
  std::vector<std::pair<routing::Edge, geometry::PointWithAltitude>> vicinity;
  m_graph.FindClosestEdges(
      mercator::RectByCenterXYAndSizeInMeters(u.m_junction.GetPoint(),
                                              routing::FeaturesRoadGraph::kClosestEdgesRadiusM),
      kMaxRoadCandidates, vicinity);

  for (auto const & p : vicinity)
  {
    auto const & edge = p.first;
    if (edge.IsFake())
      continue;
    if (!ConformLfrcnp(edge, restriction, kFRCThreshold, m_roadInfoGetter))
      continue;
    fn(edge);
  }
}

template <typename It>
size_t Router::FindPrefixLengthToConsume(It b, It const e, double lengthM)
{
  size_t n = 0;
  while (b != e && lengthM > 0.0)
  {
    // Need p here to prolongate lifetime of (*b) if iterator
    // dereferencing returns a temprorary object instead of a
    // reference.
    auto const & p = *b;
    auto const & u = p.first;
    auto const & v = p.second;
    double const len = mercator::DistanceOnEarth(u, v);
    if (2 * lengthM < len)
      break;

    lengthM -= len;
    ++n;
    ++b;
  }
  return n;
}

template <typename It>
double Router::GetCoverage(m2::PointD const & u, m2::PointD const & v, It b, It e)
{
  double const kEps = 1e-5;
  double const kLengthThresholdM = 1;

  m2::PointD const uv = v - u;
  double const sqlen = u.SquaredLength(v);

  if (mercator::DistanceOnEarth(u, v) < kLengthThresholdM)
    return 0;

  std::vector<std::pair<double, double>> covs;
  for (; b != e; ++b)
  {
    auto const s = b->m_u.m_junction.GetPoint();
    auto const t = b->m_v.m_junction.GetPoint();
    if (!m2::IsPointOnSegmentEps(s, u, v, kEps) || !m2::IsPointOnSegmentEps(t, u, v, kEps))
      continue;

    if (DotProduct(uv, t - s) < -kEps)
      continue;

    double const sp = DotProduct(uv, s - u) / sqlen;
    double const tp = DotProduct(uv, t - u) / sqlen;

    double const start = math::Clamp(std::min(sp, tp), 0.0, 1.0);
    double const finish = math::Clamp(std::max(sp, tp), 0.0, 1.0);
    covs.emplace_back(start, finish);
  }

  sort(covs.begin(), covs.end());

  double coverage = 0;

  size_t i = 0;
  while (i != covs.size())
  {
    size_t j = i;

    double const first = covs[i].first;
    double last = covs[i].second;
    while (j != covs.size() && covs[j].first <= last)
    {
      last = std::max(last, covs[j].second);
      ++j;
    }

    coverage += last - first;
    i = j;
  }

  CHECK_LESS_OR_EQUAL(coverage, 1.0 + kEps, ());

  return coverage;
}

template <typename It>
double Router::GetMatchingScore(m2::PointD const & u, m2::PointD const & v, It b, It e)
{
  double const kEps = 1e-5;

  double const len = mercator::DistanceOnEarth(u, v);

  m2::PointD const uv = v - u;

  double cov = 0;
  for (; b != e; ++b)
  {
    // Need p here to prolongate lifetime of (*b) if iterator
    // dereferencing returns a temprorary object instead of a
    // reference.
    auto const & p = *b;
    auto const & s = p.first;
    auto const & t = p.second;
    if (!m2::IsPointOnSegmentEps(s, u, v, kEps) || !m2::IsPointOnSegmentEps(t, u, v, kEps))
      break;

    m2::PointD const st = t - s;
    if (DotProduct(uv, st) < -kEps)
      break;

    cov += mercator::DistanceOnEarth(s, t);
  }

  return len == 0 ? 0 : math::Clamp(cov / len, 0.0, 1.0);
}

template <typename It, typename Fn>
void Router::ForStagePrefix(It b, It e, size_t stage, Fn && fn)
{
  while (b != e && b->m_raw.IsFake() && b->m_u.m_stage == stage && b->m_v.m_stage == stage)
    ++b;
  if (b != e && !b->m_raw.IsFake())
    fn(b);
}

bool Router::ReconstructPath(std::vector<Edge> & edges, Path & path)
{
  CHECK_GREATER_OR_EQUAL(m_points.size(), 2, ());

  using EdgeIt = std::vector<Edge>::iterator;
  using EdgeItRev = std::vector<Edge>::reverse_iterator;

  double const kFakeCoverageThreshold = 0.5;

  base::EraseIf(edges, [](auto && e) { return e.IsSpecial(); });

  {
    auto toPair = [](auto && e) { return e.ToPair(); };
    size_t const n = FindPrefixLengthToConsume(
        make_transform_iterator(edges.begin(), toPair),
        make_transform_iterator(edges.end(), toPair), m_positiveOffsetM);
    CHECK_LESS_OR_EQUAL(n, edges.size(), ());
    edges.erase(edges.begin(), edges.begin() + n);
  }

  {
    auto toPairRev = [](auto && e) { return e.ToPairRev(); };
    size_t const n = FindPrefixLengthToConsume(
        make_transform_iterator(edges.rbegin(), toPairRev),
        make_transform_iterator(edges.rend(), toPairRev), m_negativeOffsetM);
    CHECK_LESS_OR_EQUAL(n, edges.size(), ());
    edges.erase(edges.begin() + edges.size() - n, edges.end());
  }

  double frontEdgeScore = -1.0;
  routing::Edge frontEdge;
  ForStagePrefix(edges.begin(), edges.end(), 0, [&](EdgeIt e) {
    ForEachNonFakeEdge(e->m_u, false /* outgoing */, m_points[0].m_lfrcnp,
                       [&](routing::Edge const & edge) {
                         auto toPairRev = [](auto && e) { return e.ToPairRev(); };
                         double const score = GetMatchingScore(
                             edge.GetEndJunction().GetPoint(), edge.GetStartJunction().GetPoint(),
                             make_transform_iterator(EdgeItRev(e), toPairRev),
                             make_transform_iterator(edges.rend(), toPairRev));
                         if (score > frontEdgeScore)
                         {
                           frontEdgeScore = score;
                           frontEdge = edge.GetReverseEdge();
                         }
                       });
  });

  double backEdgeScore = -1.0;
  routing::Edge backEdge;
  ForStagePrefix(edges.rbegin(), edges.rend(), m_points.size() - 2 /* stage */, [&](EdgeItRev e) {
    ForEachNonFakeEdge(e->m_v, true /* outgoing */, m_points[m_points.size() - 2].m_lfrcnp,
                       [&](routing::Edge const & edge) {
                         auto toPair = [](auto && e) { return e.ToPair(); };
                         double const score = GetMatchingScore(
                             edge.GetStartJunction().GetPoint(), edge.GetEndJunction().GetPoint(),
                             make_transform_iterator(e.base(), toPair),
                             make_transform_iterator(edges.end(), toPair));
                         if (score > backEdgeScore)
                         {
                           backEdgeScore = score;
                           backEdge = edge;
                         }
                       });
  });

  path.clear();
  for (auto const & e : edges)
  {
    if (!e.IsFake())
      path.push_back(e.m_raw);
  }

  if (frontEdgeScore >= kFakeCoverageThreshold && !path.empty() && path.front() != frontEdge)
    path.insert(path.begin(), frontEdge);

  if (backEdgeScore >= kFakeCoverageThreshold && !path.empty() && path.back() != backEdge)
    path.insert(path.end(), backEdge);

  if (path.empty())
  {
    // This is the case for routes from fake edges only.
    FindSingleEdgeApproximation(edges, path);
  }

  return !path.empty();
}

void Router::FindSingleEdgeApproximation(std::vector<Edge> const & edges, Path & path)
{
  double const kCoverageThreshold = 0.5;

  CHECK(all_of(edges.begin(), edges.end(), [](auto && e) { return e.IsFake(); }), ());

  double expectedLength = 0;
  for (auto const & edge : edges)
    expectedLength += GetWeight(edge);

  if (expectedLength < kEps)
    return;

  double bestCoverage = -1.0;
  routing::Edge bestEdge;

  auto checkEdge = [&](routing::Edge const & edge) {
    double const weight = GetWeight(edge);
    double const fraction =
        GetCoverage(edge.GetStartJunction().GetPoint(), edge.GetEndJunction().GetPoint(),
                    edges.begin(), edges.end());
    double const coverage = weight * fraction;
    if (coverage >= bestCoverage)
    {
      bestCoverage = coverage;
      bestEdge = edge;
    }
  };

  for (auto const & edge : edges)
  {
    auto const & u = edge.m_u;
    auto const & v = edge.m_v;
    CHECK_EQUAL(u.m_stage, v.m_stage, ());
    auto const stage = u.m_stage;

    ForEachNonFakeClosestEdge(u, m_points[stage].m_lfrcnp, checkEdge);
    ForEachNonFakeClosestEdge(v, m_points[stage].m_lfrcnp, checkEdge);
  }

  if (bestCoverage >= expectedLength * kCoverageThreshold)
    path = {bestEdge};
}
}  // namespace openlr
