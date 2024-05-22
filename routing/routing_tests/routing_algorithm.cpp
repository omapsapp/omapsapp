#include "routing/routing_tests/routing_algorithm.hpp"

#include "routing/base/astar_algorithm.hpp"
#include "routing/base/astar_graph.hpp"

#include "routing/maxspeeds.hpp"
#include "routing/routing_helpers.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace routing_test
{
void UndirectedGraph::AddEdge(Vertex u, Vertex v, Weight w)
{
  m_adjs[u].emplace_back(v, w);
  m_adjs[v].emplace_back(u, w);
}

size_t UndirectedGraph::GetNodesNumber() const
{
  return m_adjs.size();
}

void UndirectedGraph::GetEdgesList(Vertex const & vertex, bool /* isOutgoing */,
                                   EdgeListT & adj)
{
  GetAdjacencyList(vertex, adj);
}

void UndirectedGraph::GetIngoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                                          EdgeListT & adj)
{
  GetEdgesList(vertexData.m_vertex, false /* isOutgoing */, adj);
}

void UndirectedGraph::GetOutgoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                                           EdgeListT & adj)
{
  GetEdgesList(vertexData.m_vertex, true /* isOutgoing */, adj);
}

double UndirectedGraph::HeuristicCostEstimate(Vertex const &, Vertex const &)
{
  return 0.0;
}

void UndirectedGraph::GetAdjacencyList(Vertex v, EdgeListT & adj) const
{
  adj.clear();
  auto const it = m_adjs.find(v);
  if (it != m_adjs.cend())
    adj = it->second;
}

void DirectedGraph::AddEdge(Vertex from, Vertex to, Weight w)
{
  m_outgoing[from].emplace_back(to, w);
  m_ingoing[to].emplace_back(from, w);
}

void DirectedGraph::GetEdgesList(Vertex const & v, bool isOutgoing, EdgeListT & adj)
{
  adj = isOutgoing ? m_outgoing[v] : m_ingoing[v];
}

namespace
{
inline double TimeBetweenSec(geometry::PointWithAltitude const & j1,
                             geometry::PointWithAltitude const & j2, double speedMPS)
{
  ASSERT(speedMPS > 0.0, ());
  ASSERT_NOT_EQUAL(j1.GetAltitude(), geometry::kInvalidAltitude, ());
  ASSERT_NOT_EQUAL(j2.GetAltitude(), geometry::kInvalidAltitude, ());

  double const distanceM = mercator::DistanceOnEarth(j1.GetPoint(), j2.GetPoint());
  double const altitudeDiffM =
      static_cast<double>(j2.GetAltitude()) - static_cast<double>(j1.GetAltitude());
  return std::sqrt(distanceM * distanceM + altitudeDiffM * altitudeDiffM) / speedMPS;
}

/// A class which represents an weighted edge used by RoadGraph.
class WeightedEdge
{
public:
  WeightedEdge() = default;   // needed for buffer_vector only
  WeightedEdge(geometry::PointWithAltitude const & target, double weight)
    : target(target), weight(weight)
  {
  }

  inline geometry::PointWithAltitude const & GetTarget() const { return target; }
  inline double GetWeight() const { return weight; }

private:
  geometry::PointWithAltitude target;
  double weight;
};

using Algorithm = AStarAlgorithm<geometry::PointWithAltitude, WeightedEdge, double>;

/// A wrapper around IRoadGraph, which makes it possible to use IRoadGraph with astar algorithms.
class RoadGraph : public Algorithm::Graph
{
public:

  explicit RoadGraph(RoadGraphIFace const & roadGraph)
    : m_roadGraph(roadGraph), m_maxSpeedMPS(measurement_utils::KmphToMps(roadGraph.GetMaxSpeedKMpH()))
  {}

  void GetOutgoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                            EdgeListT & adj) override
  {
    auto const & v = vertexData.m_vertex;
    IRoadGraph::EdgeListT edges;
    m_roadGraph.GetOutgoingEdges(v, edges);

    adj.clear();
    adj.reserve(edges.size());

    for (auto const & e : edges)
    {
      ASSERT_EQUAL(v, e.GetStartJunction(), ());

      double const speedMPS = measurement_utils::KmphToMps(
          m_roadGraph.GetSpeedKMpH(e, {true /* forward */, false /* in city */, Maxspeed()}));
      adj.emplace_back(e.GetEndJunction(), TimeBetweenSec(e.GetStartJunction(), e.GetEndJunction(), speedMPS));
    }
  }

  void GetIngoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                           EdgeListT & adj) override
  {
    auto const & v = vertexData.m_vertex;
    IRoadGraph::EdgeListT edges;
    m_roadGraph.GetIngoingEdges(v, edges);

    adj.clear();
    adj.reserve(edges.size());

    for (auto const & e : edges)
    {
      ASSERT_EQUAL(v, e.GetEndJunction(), ());

      double const speedMPS = measurement_utils::KmphToMps(
          m_roadGraph.GetSpeedKMpH(e, {true /* forward */, false /* in city */, Maxspeed()}));
      adj.emplace_back(e.GetStartJunction(), TimeBetweenSec(e.GetStartJunction(), e.GetEndJunction(), speedMPS));
    }
  }

  double HeuristicCostEstimate(Vertex const & v, Vertex const & w) override
  {
    return TimeBetweenSec(v, w, m_maxSpeedMPS);
  }

private:
  RoadGraphIFace const & m_roadGraph;
  double const m_maxSpeedMPS;
};

TestAStarBidirectionalAlgo::Result Convert(Algorithm::Result value)
{
  switch (value)
  {
  case Algorithm::Result::OK: return TestAStarBidirectionalAlgo::Result::OK;
  case Algorithm::Result::NoPath: return TestAStarBidirectionalAlgo::Result::NoPath;
  case Algorithm::Result::Cancelled: return TestAStarBidirectionalAlgo::Result::Cancelled;
  }

  UNREACHABLE();
  return TestAStarBidirectionalAlgo::Result::NoPath;
}
}  // namespace

std::string DebugPrint(TestAStarBidirectionalAlgo::Result const & value)
{
  switch (value)
  {
  case TestAStarBidirectionalAlgo::Result::OK:
    return "OK";
  case TestAStarBidirectionalAlgo::Result::NoPath:
    return "NoPath";
  case TestAStarBidirectionalAlgo::Result::Cancelled:
    return "Cancelled";
  }

  UNREACHABLE();
  return std::string();
}

// *************************** AStar-bidirectional routing algorithm implementation ***********************
TestAStarBidirectionalAlgo::Result TestAStarBidirectionalAlgo::CalculateRoute(
    RoadGraphIFace const & graph, geometry::PointWithAltitude const & startPos,
    geometry::PointWithAltitude const & finalPos,
    RoutingResult<IRoadGraph::Vertex, IRoadGraph::Weight> & path)
{
  RoadGraph roadGraph(graph);
  base::Cancellable cancellable;
  Algorithm::Params<> params(roadGraph, startPos, finalPos, cancellable);

  Algorithm::Result const res = Algorithm().FindPathBidirectional(params, path);
  return Convert(res);
}
}  // namespace routing_tests
