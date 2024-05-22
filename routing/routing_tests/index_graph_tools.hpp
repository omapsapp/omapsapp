#pragma once

#include "generator/generator_tests_support/routing_helpers.hpp"

#include "routing/base/astar_algorithm.hpp"
#include "routing/edge_estimator.hpp"
#include "routing/fake_ending.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_loader.hpp"
#include "routing/index_graph_starter.hpp"
#include "routing/restrictions_serialization.hpp"
#include "routing/road_access.hpp"
#include "routing/road_point.hpp"
#include "routing/route.hpp"
#include "routing/segment.hpp"
#include "routing/single_vehicle_world_graph.hpp"
#include "routing/speed_camera_ser_des.hpp"
#include "routing/transit_graph_loader.hpp"
#include "routing/transit_world_graph.hpp"

#include "traffic/traffic_info.hpp"

#include "transit/transit_types.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "indexer/classificator_loader.hpp"

#include "geometry/point2d.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace routing_test
{
using namespace routing;

// The value doesn't matter, it doesn't associated with any real mwm id.
// It just a noticeable value to detect the source of such id while debugging unit tests.
NumMwmId constexpr kTestNumMwmId = 777;

using AlgorithmForWorldGraph = AStarAlgorithm<Segment, SegmentEdge, RouteWeight>;

class WorldGraphForAStar : public AStarGraph<Segment, SegmentEdge, RouteWeight>
{
public:
  explicit WorldGraphForAStar(std::unique_ptr<SingleVehicleWorldGraph> graph) : m_graph(std::move(graph)) {}
  ~WorldGraphForAStar() override = default;

  // AStarGraph overrides:
  // @{
  Weight HeuristicCostEstimate(Vertex const & from, Vertex const & to) override
  {
    return m_graph->HeuristicCostEstimate(m_graph->GetPoint(from, true /* front */),
                                          m_graph->GetPoint(to, true /* front */));
  }

  void GetOutgoingEdgesList(astar::VertexData<Vertex, RouteWeight> const & vertexData,
                            EdgeListT & edges) override
  {
    edges.clear();
    m_graph->GetEdgeList(vertexData, true /* isOutgoing */, true /* useRoutingOptions */,
                         true /* useAccessConditional */, edges);
  }

  void GetIngoingEdgesList(astar::VertexData<Vertex, RouteWeight> const & vertexData,
                           EdgeListT & edges) override
  {
    edges.clear();
    m_graph->GetEdgeList(vertexData, false /* isOutgoing */, true /* useRoutingOptions */,
                         true /* useAccessConditional */, edges);
  }

  RouteWeight GetAStarWeightEpsilon() override { return RouteWeight(0.0); }
  // @}

  WorldGraph & GetWorldGraph() { return *m_graph; }

private:
  std::unique_ptr<SingleVehicleWorldGraph> m_graph;
};

struct RestrictionTest
{
  RestrictionTest() { classificator::Load(); }
  void Init(std::unique_ptr<SingleVehicleWorldGraph> graph) { m_graph = std::move(graph); }
  void SetStarter(FakeEnding const & start, FakeEnding const & finish);
  void SetRestrictions(RestrictionVec && restrictions);
  void SetUTurnRestrictions(std::vector<RestrictionUTurn> && restrictions);

  std::unique_ptr<SingleVehicleWorldGraph> m_graph;
  std::unique_ptr<IndexGraphStarter> m_starter;
};

struct NoUTurnRestrictionTest
{
  NoUTurnRestrictionTest() { classificator::Load(); }
  void Init(std::unique_ptr<SingleVehicleWorldGraph> graph);

  void SetRestrictions(RestrictionVec && restrictions);
  void SetNoUTurnRestrictions(std::vector<RestrictionUTurn> && restrictions);

  void TestRouteGeom(Segment const & start, Segment const & finish,
                     AlgorithmForWorldGraph::Result expectedRouteResult,
                     std::vector<m2::PointD> const & expectedRouteGeom);

  std::unique_ptr<WorldGraphForAStar> m_graph;
};

class ZeroGeometryLoader final : public routing::GeometryLoader
{
public:
  // GeometryLoader overrides:
  ~ZeroGeometryLoader() override = default;

  void Load(uint32_t featureId, routing::RoadGeometry & road) override;
};

class TestIndexGraphLoader final : public IndexGraphLoader
{
public:
  // IndexGraphLoader overrides:
  ~TestIndexGraphLoader() override = default;

  IndexGraph & GetIndexGraph(NumMwmId mwmId) override;
  Geometry & GetGeometry(NumMwmId mwmId) override;

  std::vector<RouteSegment::SpeedCamera> GetSpeedCameraInfo(Segment const &) override
  {
    return {};
  }

  void Clear() override;

  void AddGraph(NumMwmId mwmId, std::unique_ptr<IndexGraph> graph);

private:
  std::unordered_map<NumMwmId, std::unique_ptr<IndexGraph>> m_graphs;
};

class TestTransitGraphLoader : public TransitGraphLoader
{
public:
  // TransitGraphLoader overrides:
  ~TestTransitGraphLoader() override = default;

  TransitGraph & GetTransitGraph(NumMwmId mwmId, IndexGraph & indexGraph) override;
  void Clear() override;

  void AddGraph(NumMwmId mwmId, std::unique_ptr<TransitGraph> graph);

private:
  std::unordered_map<NumMwmId, std::unique_ptr<TransitGraph>> m_graphs;
};

// An estimator that uses the information from the supported |segmentWeights| map
// and completely ignores road geometry. The underlying graph is assumed
// to be directed, i.e. it is not guaranteed that each segment has its reverse
// in the map.
class WeightedEdgeEstimator final : public EdgeEstimator
{
public:
  // maxSpeedKMpH doesn't matter, but should be greater then any road speed in all tests.
  // offroadSpeedKMpH doesn't matter, but should be > 0 and <= maxSpeedKMpH.
  explicit WeightedEdgeEstimator(std::map<Segment, double> const & segmentWeights)
    : EdgeEstimator(1e10 /* maxSpeedKMpH */, 1.0 /* offroadSpeedKMpH */)
    , m_segmentWeights(segmentWeights)
  {
  }

  // EdgeEstimator overrides:
  ~WeightedEdgeEstimator() override = default;

  double CalcSegmentWeight(Segment const & segment, RoadGeometry const & /* road */,
                           EdgeEstimator::Purpose purpose) const override;

  double GetUTurnPenalty(Purpose purpose) const override;
  double GetFerryLandingPenalty(Purpose purpose) const override;

private:
  std::map<Segment, double> m_segmentWeights;
};

// A simple class to test graph algorithms for the index graph
// that do not depend on road geometry (but may depend on the
// lengths of roads).
class TestIndexGraphTopology final
{
public:
  using Vertex = uint32_t;
  using Edge = std::pair<Vertex, Vertex>;

  // Creates an empty graph on |numVertices| vertices.
  TestIndexGraphTopology(uint32_t numVertices);

  // Adds a weighted directed edge to the graph. Multi-edges are not supported.
  // *NOTE* The edges are added lazily, i.e. edge requests are only stored here
  // and the graph itself is built only after a call to FindPath.
  void AddDirectedEdge(Vertex from, Vertex to, double weight);

  // Sets access for previously added edge.
  void SetEdgeAccess(Vertex from, Vertex to, RoadAccess::Type type);
  /// \param |condition| in osm opening hours format.
  void SetEdgeAccessConditional(Vertex from, Vertex to, RoadAccess::Type type,
                                std::string const & condition);

  // Sets access type for previously added point.
  void SetVertexAccess(Vertex v, RoadAccess::Type type);
  /// \param |condition| in osm opening hours format.
  void SetVertexAccessConditional(Vertex v, RoadAccess::Type type, std::string const & condition);

  // Finds a path between the start and finish vertices. Returns true iff a path exists.
  bool FindPath(Vertex start, Vertex finish, double & pathWeight,
                std::vector<Edge> & pathEdges) const;

  template <typename T>
  void SetCurrentTimeGetter(T && getter) { m_currentTimeGetter = std::forward<T>(getter); }

private:
  struct EdgeRequest
  {
    uint32_t m_id = 0;
    Vertex m_from = 0;
    Vertex m_to = 0;
    double m_weight = 0.0;
    // Access type for edge.
    RoadAccess::Type m_accessType = RoadAccess::Type::Yes;
    RoadAccess::Conditional m_accessConditionalType;
    // Access type for vertex from.
    RoadAccess::Type m_fromAccessType = RoadAccess::Type::Yes;
    RoadAccess::Conditional m_fromAccessConditionalType;
    // Access type for vertex to.
    RoadAccess::Type m_toAccessType = RoadAccess::Type::Yes;
    RoadAccess::Conditional m_toAccessConditionalType;

    EdgeRequest(uint32_t id, Vertex from, Vertex to, double weight)
      : m_id(id), m_from(from), m_to(to), m_weight(weight)
    {
    }
  };

  // Builder builds a graph from edge requests.
  struct Builder
  {
    explicit Builder(uint32_t numVertices) : m_numVertices(numVertices) {}
    std::unique_ptr<SingleVehicleWorldGraph> PrepareIndexGraph();
    void BuildJoints();
    void BuildGraphFromRequests(std::vector<EdgeRequest> const & requests);
    void BuildSegmentFromEdge(EdgeRequest const & request);
    void SetCurrentTimeGetter(std::function<time_t()> const & getter) { m_currentTimeGetter = getter; }

    uint32_t const m_numVertices;
    std::map<Edge, double> m_edgeWeights;
    std::map<Segment, double> m_segmentWeights;
    std::map<Segment, Edge> m_segmentToEdge;
    std::map<Vertex, std::vector<Segment>> m_outgoingSegments;
    std::map<Vertex, std::vector<Segment>> m_ingoingSegments;
    std::vector<Joint> m_joints;
    RoadAccess m_roadAccess;
    std::function<time_t()> m_currentTimeGetter;
  };

  void AddDirectedEdge(std::vector<EdgeRequest> & edgeRequests, Vertex from, Vertex to,
                       double weight) const;

  std::function<time_t()> m_currentTimeGetter;
  uint32_t const m_numVertices;
  std::vector<EdgeRequest> m_edgeRequests;
};

std::unique_ptr<SingleVehicleWorldGraph> BuildWorldGraph(std::unique_ptr<TestGeometryLoader> loader,
                                                         std::shared_ptr<EdgeEstimator> estimator,
                                                         std::vector<Joint> const & joints);
std::unique_ptr<SingleVehicleWorldGraph> BuildWorldGraph(std::unique_ptr<ZeroGeometryLoader> loader,
                                                         std::shared_ptr<EdgeEstimator> estimator,
                                                         std::vector<Joint> const & joints);

AStarAlgorithm<Segment, SegmentEdge, RouteWeight>::Result CalculateRoute(
    IndexGraphStarter & starter, std::vector<Segment> & roadPoints, double & timeSec);

void TestRouteGeometry(
    IndexGraphStarter & starter,
    AStarAlgorithm<Segment, SegmentEdge, RouteWeight>::Result expectedRouteResult,
    std::vector<m2::PointD> const & expectedRouteGeom);

/// \brief Applies |restrictions| to graph in |restrictionTest| and
/// tests the resulting route.
/// \note restrictionTest should have a valid |restrictionTest.m_graph|.
void TestRestrictions(std::vector<m2::PointD> const & expectedRouteGeom,
                      AStarAlgorithm<Segment, SegmentEdge, RouteWeight>::Result expectedRouteResult,
                      FakeEnding const & start, FakeEnding const & finish,
                      RestrictionVec && restrictions, RestrictionTest & restrictionTest);

void TestRestrictions(std::vector<m2::PointD> const & expectedRouteGeom,
                      AStarAlgorithm<Segment, SegmentEdge, RouteWeight>::Result expectedRouteResult,
                      FakeEnding const & start, FakeEnding const & finish,
                      RestrictionVec && restrictions,
                      std::vector<RestrictionUTurn> && restrictionsNoUTurn,
                      RestrictionTest & restrictionTest);

void TestRestrictions(double timeExpected,
                      FakeEnding const & start, FakeEnding const & finish,
                      RestrictionVec && restrictions, RestrictionTest & restrictionTest);

// Tries to find a unique path from |from| to |to| in |graph|.
// If |expectedPathFound| is true, |expectedWeight| and |expectedEdges| must
// specify the weight and edges of the unique shortest path.
// If |expectedPathFound| is false, |expectedWeight| and |expectedEdges| may
// take arbitrary values.
void TestTopologyGraph(TestIndexGraphTopology const & graph, TestIndexGraphTopology::Vertex from,
                       TestIndexGraphTopology::Vertex to, bool expectedPathFound,
                       double expectedWeight,
                       std::vector<TestIndexGraphTopology::Edge> const & expectedEdges);

// Creates FakeEnding projected to |Segment(kTestNumMwmId, featureId, segmentIdx, true /* forward
// */)|.
FakeEnding MakeFakeEnding(uint32_t featureId, uint32_t segmentIdx, m2::PointD const & point,
                          WorldGraph & graph);

std::unique_ptr<IndexGraphStarter> MakeStarter(FakeEnding const & start, FakeEnding const & finish,
                                               WorldGraph & graph);

using Month = osmoh::MonthDay::Month;
using Weekday = osmoh::Weekday;

time_t GetUnixtimeByDate(uint16_t year, Month month, uint8_t monthDay, uint8_t hours,
                         uint8_t minutes);
time_t GetUnixtimeByDate(uint16_t year, Month month, Weekday weekday, uint8_t hours,
                         uint8_t minutes);
}  // namespace routing_test
