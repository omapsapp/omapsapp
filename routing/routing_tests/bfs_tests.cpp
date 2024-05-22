#include "testing/testing.hpp"

#include "routing/base/bfs.hpp"

#include "routing/routing_tests/routing_algorithm.hpp"

#include <cstdint>
#include <set>
#include <vector>

namespace
{
using namespace routing_test;

double constexpr kWeight = 1.0;

UndirectedGraph BuildUndirectedGraph()
{
  UndirectedGraph graph;

  // Inserts edges in a format: <source, target, weight>.
  graph.AddEdge(0, 4, kWeight);
  graph.AddEdge(5, 4, kWeight);
  graph.AddEdge(4, 1, kWeight);
  graph.AddEdge(4, 3, kWeight);
  graph.AddEdge(3, 2, kWeight);
  graph.AddEdge(7, 4, kWeight);
  graph.AddEdge(7, 6, kWeight);
  graph.AddEdge(7, 8, kWeight);
  graph.AddEdge(8, 9, kWeight);
  graph.AddEdge(8, 10, kWeight);

  return graph;
}

DirectedGraph BuildDirectedGraph()
{
  DirectedGraph graph;

  // Inserts edges in a format: <source, target, weight>.
  graph.AddEdge(0, 4, kWeight);
  graph.AddEdge(5, 4, kWeight);
  graph.AddEdge(4, 1, kWeight);
  graph.AddEdge(4, 3, kWeight);
  graph.AddEdge(3, 2, kWeight);
  graph.AddEdge(7, 4, kWeight);
  graph.AddEdge(7, 6, kWeight);
  graph.AddEdge(7, 8, kWeight);
  graph.AddEdge(8, 9, kWeight);
  graph.AddEdge(8, 10, kWeight);

  return graph;
}

DirectedGraph BuildDirectedCyclicGraph()
{
  DirectedGraph graph;

  // Inserts edges in a format: <source, target, weight>.
  graph.AddEdge(0, 1, kWeight);
  graph.AddEdge(1, 2, kWeight);
  graph.AddEdge(2, 3, kWeight);
  graph.AddEdge(3, 4, kWeight);
  graph.AddEdge(4, 2, kWeight);

  return graph;
}

DirectedGraph BuildSmallDirectedCyclicGraph()
{
  DirectedGraph graph;

  // Inserts edges in a format: <source, target, weight>.
  graph.AddEdge(0, 1, kWeight);
  graph.AddEdge(1, 2, kWeight);
  graph.AddEdge(2, 0, kWeight);

  return graph;
}
}  // namespace

namespace routing_test
{
using namespace routing;

UNIT_TEST(BFS_AllVisit_Undirected)
{
  UndirectedGraph graph = BuildUndirectedGraph();

  std::set<uint32_t> visited;

  BFS<UndirectedGraph> bfs(graph);
  bfs.Run(0 /* start */, true /* isOutgoing */,
      [&](BFS<UndirectedGraph>::State const & state)
      {
        visited.emplace(state.m_vertex);
        return true;
      });

  std::vector<uint32_t> const expectedInVisited = {1, 2, 3, 4 ,5, 6, 7, 8, 9, 10};
  for (auto const v : expectedInVisited)
    TEST_NOT_EQUAL(visited.count(v), 0, ("vertex =", v, "was not visited."));
}

UNIT_TEST(BFS_AllVisit_Directed_Forward)
{
  DirectedGraph graph = BuildDirectedGraph();

  std::set<uint32_t> visited;

  BFS<DirectedGraph> bfs(graph);
  bfs.Run(0 /* start */, true /* isOutgoing */,
          [&](BFS<DirectedGraph>::State const & state)
          {
            visited.emplace(state.m_vertex);
            return true;
          });

  std::vector<uint32_t> const expectedInVisited = {1, 2, 3, 4};
  for (auto const v : expectedInVisited)
    TEST_NOT_EQUAL(visited.count(v), 0, ("vertex =", v, "was not visited."));
}

UNIT_TEST(BFS_AllVisit_Directed_Backward)
{
  DirectedGraph graph = BuildDirectedGraph();

  std::set<uint32_t> visited;

  BFS<DirectedGraph> bfs(graph);
  bfs.Run(2 /* start */, false /* isOutgoing */,
          [&](BFS<DirectedGraph>::State const & state)
          {
            visited.emplace(state.m_vertex);
            return true;
          });

  std::vector<uint32_t> expectedInVisited = {0, 3, 4, 5, 7};
  for (auto const v : expectedInVisited)
    TEST_NOT_EQUAL(visited.count(v), 0, ("vertex =", v, "was not visited."));
}

UNIT_TEST(BFS_AllVisit_DirectedCyclic)
{
  DirectedGraph graph = BuildDirectedCyclicGraph();

  std::set<uint32_t> visited;

  BFS<DirectedGraph> bfs(graph);
  bfs.Run(0 /* start */, true /* isOutgoing */,
          [&](BFS<DirectedGraph>::State const & state)
          {
            visited.emplace(state.m_vertex);
            return true;
          });

  std::vector<uint32_t> expectedInVisited = {1, 2, 3, 4};
  for (auto const v : expectedInVisited)
    TEST_NOT_EQUAL(visited.count(v), 0, ("vertex =", v, "was not visited."));
}

UNIT_TEST(BFS_ReconstructPathTest)
{
  DirectedGraph graph = BuildSmallDirectedCyclicGraph();

  BFS<DirectedGraph> bfs(graph);
  bfs.Run(0 /* start */, true /* isOutgoing */, [&](auto const &) { return true; });

  std::vector<uint32_t> path = bfs.ReconstructPath(2, false /* reverse */);
  std::vector<uint32_t> expected = {0, 1, 2};
  TEST_EQUAL(path, expected, ());

  path = bfs.ReconstructPath(2, true /* reverse */);
  expected = {2, 1, 0};
  TEST_EQUAL(path, expected, ());
}
} //  namespace routing_test
