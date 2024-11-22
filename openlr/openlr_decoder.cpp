#include "openlr/openlr_decoder.hpp"

#include "openlr/cache_line_size.hpp"
#include "openlr/candidate_paths_getter.hpp"
#include "openlr/candidate_points_getter.hpp"
#include "openlr/decoded_path.hpp"
#include "openlr/graph.hpp"
#include "openlr/helpers.hpp"
#include "openlr/openlr_model.hpp"
#include "openlr/paths_connector.hpp"
#include "openlr/road_info_getter.hpp"
#include "openlr/router.hpp"
#include "openlr/score_candidate_paths_getter.hpp"
#include "openlr/score_candidate_points_getter.hpp"
#include "openlr/score_paths_connector.hpp"
#include "openlr/score_types.hpp"
#include "openlr/way_point.hpp"

#include "routing/features_road_graph.hpp"
#include "routing/road_graph.hpp"

#include "routing_common/car_model.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/classificator.hpp"
#include "indexer/data_source.hpp"

#include "platform/country_file.hpp"

#include "geometry/mercator.hpp"
#include "geometry/parametrized_segment.hpp"
#include "geometry/point2d.hpp"
#include "geometry/polyline2d.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/stl_helpers.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <queue>
#include <thread>
#include <utility>

using namespace routing;
using namespace std;

namespace openlr
{
namespace
{
struct alignas(kCacheLineSize) Stats
{
  void Add(Stats const & rhs)
  {
    m_shortRoutes += rhs.m_shortRoutes;
    m_zeroCanditates += rhs.m_zeroCanditates;
    m_moreThanOneCandidate += rhs.m_moreThanOneCandidate;
    m_routesFailed += rhs.m_routesFailed;
    m_tightOffsets += rhs.m_tightOffsets;
    m_routesHandled += rhs.m_routesHandled;
  }

  void Report() const
  {
    LOG(LINFO, ("Parsed segments:", m_routesHandled));
    LOG(LINFO, ("Routes failed:", m_routesFailed));
    LOG(LINFO, ("Tight offsets:", m_tightOffsets));
    LOG(LINFO, ("Short routes:", m_shortRoutes));
    LOG(LINFO, ("Ambiguous routes:", m_moreThanOneCandidate));
    LOG(LINFO, ("Path is not reconstructed:", m_zeroCanditates));
  }

  uint32_t m_shortRoutes = 0;
  uint32_t m_zeroCanditates = 0;
  uint32_t m_moreThanOneCandidate = 0;
  uint32_t m_routesFailed = 0;
  uint32_t m_tightOffsets = 0;
  uint32_t m_routesHandled = 0;
};

bool IsRealVertex(m2::PointD const & p, FeatureID const & fid, DataSource const & dataSource)
{
  FeaturesLoaderGuard g(dataSource, fid.m_mwmId);
  auto const ft = g.GetOriginalFeatureByIndex(fid.m_index);
  bool matched = false;
  ft->ForEachPoint(
      [&p, &matched](m2::PointD const & fp) {
        if (p == fp)
          matched = true;
      },
      FeatureType::BEST_GEOMETRY);
  return matched;
}

void ExpandFake(Graph::EdgeVector & path, Graph::EdgeVector::iterator edgeIt, DataSource const & dataSource,
                Graph & g)
{
  if (!edgeIt->IsFake())
    return;

  Graph::EdgeListT edges;
  bool startIsFake = true;
  if (IsRealVertex(edgeIt->GetStartPoint(), edgeIt->GetFeatureId(), dataSource))
  {
    g.GetRegularOutgoingEdges(edgeIt->GetStartJunction(), edges);
    startIsFake = false;
  }
  else
  {
    ASSERT(IsRealVertex(edgeIt->GetEndPoint(), edgeIt->GetFeatureId(), dataSource), ());
    g.GetRegularIngoingEdges(edgeIt->GetEndJunction(), edges);
  }

  CHECK(!edges.empty(), ());

  auto it = find_if(begin(edges), end(edges), [&edgeIt](Graph::Edge const & real) {
      if (real.GetFeatureId() == edgeIt->GetFeatureId() && real.GetSegId() == edgeIt->GetSegId())
        return true;
      return false;
    });

  // For features which cross mwm border FeatureIds may not match. Check geometry.
  if (it == end(edges))
  {
    it = find_if(begin(edges), end(edges), [&edgeIt, &startIsFake](Graph::Edge const & real) {
      // Features from the same mwm should be already matched.
      if (real.GetFeatureId().m_mwmId == edgeIt->GetFeatureId().m_mwmId)
        return false;

      auto const fakePoint = startIsFake ? edgeIt->GetStartPoint() : edgeIt->GetEndPoint();
      m2::ParametrizedSegment<m2::PointD> const realGeometry(real.GetStartPoint(), real.GetEndPoint());
      auto const projectedPoint = realGeometry.ClosestPointTo(fakePoint);

      auto constexpr kCrossMwmMatchDistanceM = 1.0;
      if (mercator::DistanceOnEarth(fakePoint, projectedPoint) < kCrossMwmMatchDistanceM)
        return true;
      return false;
    });
  }

  CHECK(it != end(edges), ());

  // If a fake edge is larger than a half of the corresponding real one, substitute
  // the fake one with real one. Drop the fake one otherwize.
  if (2 * EdgeLength(*edgeIt) >= EdgeLength(*it))
    *edgeIt = *it;
  else
    path.erase(edgeIt);
}

void ExpandFakes(DataSource const & dataSource, Graph & g, Graph::EdgeVector & path)
{
  ASSERT(!path.empty(), ());

  ExpandFake(path, std::begin(path), dataSource, g);
  if (path.empty())
    return;
  ExpandFake(path, std::end(path) - 1, dataSource, g);
}

// Returns an iterator pointing to the first edge that should not be cut off.
// Offsets denote a distance in meters one should travel from the start/end of the path
// to some point along that path and drop everything form the start to that point or from
// that point to the end.
template <typename InputIterator>
InputIterator CutOffset(InputIterator start, InputIterator stop, double offset,
                        bool keepEnd)
{
  if (offset == 0.0)
    return start;

  for (double distance = 0.0; start != stop; ++start)
  {
    auto const edgeLen = EdgeLength(*start);
    if (distance <= offset && offset < distance + edgeLen)
    {
      // Throw out this edge if (offset - distance) is greater than edgeLength / 2.
      if (!keepEnd && offset - distance >= edgeLen / 2.0)
        ++start;
      break;
    }
    distance += edgeLen;
  }

  return start;
}

template <typename InputIterator, typename OutputIterator>
void CopyWithoutOffsets(InputIterator start, InputIterator stop, OutputIterator out,
                        uint32_t positiveOffset, uint32_t negativeOffset, bool keepEnds)
{
  auto from = start;
  auto to = stop;

  if (distance(start, stop) > 1)
  {
    from = CutOffset(start, stop, positiveOffset, keepEnds);
    // |to| points past the last edge we need to take.
    to = CutOffset(reverse_iterator<InputIterator>(stop), reverse_iterator<InputIterator>(start),
                   negativeOffset, keepEnds).base();
  }

  if (!keepEnds)
    CHECK(from <= to, ("From iterator is less or equal than to."));

  if (from >= to)
    return;

  copy(from, to, out);
}


class SegmentsDecoderV2
{
public:
  SegmentsDecoderV2(DataSource & dataSource, unique_ptr<CarModelFactory> cmf)
    : m_dataSource(dataSource), m_graph(dataSource, std::move(cmf)), m_infoGetter(dataSource)
  {
  }

  bool DecodeSegment(LinearSegment const & segment, DecodedPath & path, v2::Stats & stat)
  {
    double constexpr kPathLengthTolerance = 0.30;
    uint32_t constexpr kMaxJunctionCandidates = 10;
    uint32_t constexpr kMaxProjectionCandidates = 5;

    m_graph.ResetFakes();

    path.m_segmentId.Set(segment.m_segmentId);

    auto const & points = segment.GetLRPs();
    CHECK_GREATER(points.size(), 1, ("A segment cannot consist of less than two points"));
    vector<vector<Graph::EdgeVector>> lineCandidates;
    lineCandidates.reserve(points.size());
    LOG(LDEBUG, ("Decoding segment:", segment.m_segmentId, "with", points.size(), "points"));

    CandidatePointsGetter pointsGetter(kMaxJunctionCandidates, kMaxProjectionCandidates,
                                       m_dataSource, m_graph);
    CandidatePathsGetter pathsGetter(pointsGetter, m_graph, m_infoGetter, stat);

    if (!pathsGetter.GetLineCandidatesForPoints(points, lineCandidates))
      return false;

    vector<Graph::EdgeVector> resultPath;
    PathsConnector connector(kPathLengthTolerance, m_graph, m_infoGetter, stat);
    if (!connector.ConnectCandidates(points, lineCandidates, resultPath))
      return false;

    Graph::EdgeVector route;
    for (auto const & part : resultPath)
      route.insert(end(route), begin(part), end(part));

    double requiredRouteDistanceM = 0.0;
    // Sum app all distances between points. Last point's m_distanceToNextPoint
    // should be equal to zero, but let's skip it just in case.
    CHECK(!points.empty(), ());
    for (auto it = begin(points); it != prev(end(points));  ++it)
      requiredRouteDistanceM += it->m_distanceToNextPoint;

    double actualRouteDistanceM = 0.0;
    for (auto const & e : route)
      actualRouteDistanceM += EdgeLength(e);

    auto const scale = actualRouteDistanceM / requiredRouteDistanceM;
    LOG(LDEBUG, ("actualRouteDistance:", actualRouteDistanceM,
                 "requiredRouteDistance:", requiredRouteDistanceM, "scale:", scale));

    auto const positiveOffsetM = segment.m_locationReference.m_positiveOffsetMeters * scale;
    auto const negativeOffsetM = segment.m_locationReference.m_negativeOffsetMeters * scale;

    if (positiveOffsetM + negativeOffsetM >= requiredRouteDistanceM)
    {
      ++stat.m_wrongOffsets;
      LOG(LINFO, ("Wrong offsets for segment:", segment.m_segmentId));
      return false;
    }

    ExpandFakes(m_dataSource, m_graph, route);
    ASSERT(none_of(begin(route), end(route), mem_fn(&Graph::Edge::IsFake)), (segment.m_segmentId));
    CopyWithoutOffsets(begin(route), end(route), back_inserter(path.m_path), positiveOffsetM,
                       negativeOffsetM, false /* keep ends */);

    if (path.m_path.empty())
    {
      ++stat.m_wrongOffsets;
      LOG(LINFO, ("Path is empty after offsets cutting. segmentId:", segment.m_segmentId));
      return false;
    }

    return true;
  }

private:
  DataSource const & m_dataSource;
  Graph m_graph;
  RoadInfoGetter m_infoGetter;
};

// The idea behind the third version of matching algorithm is to collect a lot of candidates (paths)
// which correspond an openlr edges with some score. And on the final stage to decide which
// candidate is better.
class SegmentsDecoderV3
{
public:
  SegmentsDecoderV3(DataSource & dataSource, unique_ptr<CarModelFactory> carModelFactory)
      : m_dataSource(dataSource), m_graph(dataSource, std::move(carModelFactory)), m_infoGetter(dataSource)
  {
  }

  bool DecodeSegment(LinearSegment const & segment, DecodedPath & path, v2::Stats & stat)
  {
    LOG(LINFO, ("DecodeSegment(...) seg id:", segment.m_segmentId, ", point num:", segment.GetLRPs().size()));

    uint32_t constexpr kMaxJunctionCandidates = 10;
    uint32_t constexpr kMaxProjectionCandidates = 5;

    path.m_segmentId.Set(segment.m_segmentId);

    auto const & points = segment.GetLRPs();
    CHECK_GREATER(points.size(), 1, ("A segment cannot consist of less than two points"));
    vector<ScorePathVec> lineCandidates;
    lineCandidates.reserve(points.size());
    LOG(LINFO, ("Decoding segment:", segment.m_segmentId, "with", points.size(), "points"));

    ScoreCandidatePointsGetter pointsGetter(kMaxJunctionCandidates, kMaxProjectionCandidates,
                                            m_dataSource, m_graph);
    ScoreCandidatePathsGetter pathsGetter(pointsGetter, m_graph, m_infoGetter, stat);

    if (!pathsGetter.GetLineCandidatesForPoints(points, segment.m_source, lineCandidates))
      return false;

    vector<Graph::EdgeVector> resultPath;
    ScorePathsConnector connector(m_graph, m_infoGetter, stat);
    if (!connector.FindBestPath(points, lineCandidates, segment.m_source, resultPath))
    {
      LOG(LINFO, ("Connections not found:", segment.m_segmentId));
      auto const mercatorPoints = segment.GetMercatorPoints();
      for (auto const & mercatorPoint : mercatorPoints)
        LOG(LINFO, (mercator::ToLatLon(mercatorPoint)));
      return false;
    }

    Graph::EdgeVector route;
    for (auto const & part : resultPath)
      route.insert(route.end(), part.begin(), part.end());

    double requiredRouteDistanceM = 0.0;
    // Sum up all distances between points. Last point's m_distanceToNextPoint
    // should be equal to zero, but let's skip it just in case.
    CHECK(!points.empty(), ());
    for (auto it = points.begin(); it != prev(points.end());  ++it)
      requiredRouteDistanceM += it->m_distanceToNextPoint;

    double actualRouteDistanceM = 0.0;
    for (auto const & e : route)
      actualRouteDistanceM += EdgeLength(e);

    auto const scale = actualRouteDistanceM / requiredRouteDistanceM;
    LOG(LINFO, ("actualRouteDistance:", actualRouteDistanceM,
        "requiredRouteDistance:", requiredRouteDistanceM, "scale:", scale));

    if (segment.m_locationReference.m_positiveOffsetMeters +
            segment.m_locationReference.m_negativeOffsetMeters >=
        requiredRouteDistanceM)
    {
      ++stat.m_wrongOffsets;
      LOG(LINFO, ("Wrong offsets for segment:", segment.m_segmentId));
      return false;
    }

    auto const positiveOffsetM = segment.m_locationReference.m_positiveOffsetMeters * scale;
    auto const negativeOffsetM = segment.m_locationReference.m_negativeOffsetMeters * scale;

    CHECK(none_of(route.begin(), route.end(), mem_fn(&Graph::Edge::IsFake)), (segment.m_segmentId));
    CopyWithoutOffsets(route.begin(), route.end(), back_inserter(path.m_path), positiveOffsetM,
                       negativeOffsetM, true /* keep ends */);

    if (path.m_path.empty())
    {
      ++stat.m_wrongOffsets;
      LOG(LINFO, ("Path is empty after offsets cutting. segmentId:", segment.m_segmentId));
      return false;
    }

    return true;
  }

private:
  DataSource const & m_dataSource;
  Graph m_graph;
  RoadInfoGetter m_infoGetter;
};

size_t constexpr GetOptimalBatchSize()
{
  // This code computes the most optimal (in the sense of cache lines
  // occupancy) batch size.
  size_t constexpr a = math::LCM(sizeof(LinearSegment), kCacheLineSize) / sizeof(LinearSegment);
  size_t constexpr b =
      math::LCM(sizeof(IRoadGraph::EdgeVector), kCacheLineSize) / sizeof(IRoadGraph::EdgeVector);
  return math::LCM(a, b);
}
}  // namespace

// OpenLRDecoder::SegmentsFilter -------------------------------------------------------------
OpenLRDecoder::SegmentsFilter::SegmentsFilter(string const & idsPath,
                                              bool const multipointsOnly)
  : m_idsSet(false), m_multipointsOnly(multipointsOnly)
{
  if (idsPath.empty())
    return;

  ifstream ifs(idsPath);
  CHECK(ifs, ("Can't find", idsPath));
  m_ids.insert(istream_iterator<uint32_t>(ifs), istream_iterator<uint32_t>());

  CHECK(!ifs, ("Garbage in", idsPath));
  m_idsSet = true;
}

bool OpenLRDecoder::SegmentsFilter::Matches(LinearSegment const & segment) const
{
  if (m_multipointsOnly && segment.m_locationReference.m_points.size() == 2)
    return false;
  if (m_idsSet && m_ids.count(segment.m_segmentId) == 0)
    return false;
  return true;
}

// OpenLRDecoder -----------------------------------------------------------------------------
OpenLRDecoder::OpenLRDecoder(vector<FrozenDataSource> & dataSources,
                             CountryParentNameGetter const & countryParentNameGetter)
  : m_dataSources(dataSources), m_countryParentNameGetter(countryParentNameGetter)
{
}

void OpenLRDecoder::DecodeV2(vector<LinearSegment> const & segments, uint32_t const numThreads,
                             vector<DecodedPath> & paths)
{
  Decode<SegmentsDecoderV2, v2::Stats>(segments, numThreads, paths);
}

void OpenLRDecoder::DecodeV3(vector<LinearSegment> const & segments, uint32_t numThreads,
                             vector<DecodedPath> & paths)
{
  Decode<SegmentsDecoderV3, v2::Stats>(segments, numThreads, paths);
}

template <typename Decoder, typename Stats>
void OpenLRDecoder::Decode(vector<LinearSegment> const & segments,
                           uint32_t const numThreads, vector<DecodedPath> & paths)
{
  auto const worker = [&](size_t threadNum, DataSource & dataSource, Stats & stat)
  {
    size_t constexpr kBatchSize = GetOptimalBatchSize();
    size_t constexpr kProgressFrequency = 100;

    size_t const numSegments = segments.size();

    Decoder decoder(dataSource, make_unique<CarModelFactory>(m_countryParentNameGetter));
    base::Timer timer;
    for (size_t i = threadNum * kBatchSize; i < numSegments; i += numThreads * kBatchSize)
    {
      for (size_t j = i; j < numSegments && j < i + kBatchSize; ++j)
      {
        if (!decoder.DecodeSegment(segments[j], paths[j], stat))
          ++stat.m_routesFailed;
        ++stat.m_routesHandled;

        if (stat.m_routesHandled % kProgressFrequency == 0 || i == segments.size() - 1)
        {
          LOG(LINFO, ("Thread", threadNum, "processed", stat.m_routesHandled,
                      "failed:", stat.m_routesFailed));
          timer.Reset();
        }
      }
    }
  };

  base::Timer timer;
  vector<Stats> stats(numThreads);
  vector<thread> workers;
  for (size_t i = 1; i < numThreads; ++i)
    workers.emplace_back(worker, i, ref(m_dataSources[i]), ref(stats[i]));

  worker(0 /* threadNum */, m_dataSources[0], stats[0]);
  for (auto & worker : workers)
    worker.join();

  Stats allStats;
  for (auto const & s : stats)
    allStats.Add(s);

  allStats.Report();
  LOG(LINFO, ("Matching tool:", timer.ElapsedSeconds(), "seconds."));
}
}  // namespace openlr
