#include "routing/routing_tests/index_graph_tools.hpp"

#include "testing/testing.hpp"

#include "routing/base/routing_result.hpp"
#include "routing/geometry.hpp"
#include "routing/routing_helpers.hpp"

#include "transit/transit_version.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"

#include <unordered_map>

#include "3party/opening_hours/opening_hours.hpp"

namespace routing_test
{
using namespace std;

double constexpr kEpsilon = 1e-6;

template <typename Graph>
Graph & GetGraph(unordered_map<NumMwmId, unique_ptr<Graph>> const & graphs, NumMwmId mwmId)
{
  auto it = graphs.find(mwmId);
  CHECK(it != graphs.end(), ("Not found graph for mwm", mwmId));
  return *it->second;
}

template <typename Graph>
void AddGraph(unordered_map<NumMwmId, unique_ptr<Graph>> & graphs, NumMwmId mwmId,
              unique_ptr<Graph> graph)
{
  auto it = graphs.find(mwmId);
  CHECK(it == graphs.end(), ("Already contains graph for mwm", mwmId));
  graphs[mwmId] = std::move(graph);
}

// RestrictionTest ---------------------------------------------------------------------------------
void RestrictionTest::SetStarter(FakeEnding const & start, FakeEnding const & finish)
{
  CHECK(m_graph != nullptr, ("Init() was not called."));
  m_starter = MakeStarter(start, finish, *m_graph);
}

void RestrictionTest::SetRestrictions(RestrictionVec && restrictions)
{
  m_graph->GetIndexGraphForTests(kTestNumMwmId).SetRestrictions(std::move(restrictions));
}

void RestrictionTest::SetUTurnRestrictions(vector<RestrictionUTurn> && restrictions)
{
  m_graph->GetIndexGraphForTests(kTestNumMwmId).SetUTurnRestrictions(std::move(restrictions));
}

// NoUTurnRestrictionTest --------------------------------------------------------------------------
void NoUTurnRestrictionTest::Init(unique_ptr<SingleVehicleWorldGraph> graph)
{
  m_graph = make_unique<WorldGraphForAStar>(std::move(graph));
}

void NoUTurnRestrictionTest::SetRestrictions(RestrictionVec && restrictions)
{
  auto & indexGraph = m_graph->GetWorldGraph().GetIndexGraph(kTestNumMwmId);
  indexGraph.SetRestrictions(std::move(restrictions));
}

void NoUTurnRestrictionTest::SetNoUTurnRestrictions(vector<RestrictionUTurn> && restrictions)
{
  auto & indexGraph = m_graph->GetWorldGraph().GetIndexGraph(kTestNumMwmId);
  indexGraph.SetUTurnRestrictions(std::move(restrictions));
}

void NoUTurnRestrictionTest::TestRouteGeom(Segment const & start, Segment const & finish,
                                           AlgorithmForWorldGraph::Result expectedRouteResult,
                                           vector<m2::PointD> const & expectedRouteGeom)
{
  AlgorithmForWorldGraph algorithm;
  AlgorithmForWorldGraph::ParamsForTests<> params(*m_graph, start, finish);

  RoutingResult<Segment, RouteWeight> routingResult;
  auto const resultCode = algorithm.FindPathBidirectional(params, routingResult);

  TEST_EQUAL(resultCode, expectedRouteResult, ());
  for (size_t i = 0; i < routingResult.m_path.size(); ++i)
  {
    static auto constexpr kEps = 1e-3;
    auto const point = m_graph->GetWorldGraph().GetPoint(routingResult.m_path[i], true /* forward */);
    if (!AlmostEqualAbs(mercator::FromLatLon(point), expectedRouteGeom[i], kEps))
    {
      TEST(false, ("Coords missmated at index:", i, "expected:", expectedRouteGeom[i],
                   "received:", point));
    }
  }
}

// ZeroGeometryLoader ------------------------------------------------------------------------------
void ZeroGeometryLoader::Load(uint32_t /* featureId */, routing::RoadGeometry & road)
{
  // Any valid road will do.
  auto const points = routing::RoadGeometry::Points({{0.0, 0.0}, {0.0, 1.0}});
  road = RoadGeometry(true /* oneWay */, 1.0 /* weightSpeedKMpH */, 1.0 /* etaSpeedKMpH */, points);
}

// TestIndexGraphLoader ----------------------------------------------------------------------------
IndexGraph & TestIndexGraphLoader::GetIndexGraph(NumMwmId mwmId)
{
  return GetGraph(m_graphs, mwmId);
}

Geometry & TestIndexGraphLoader::GetGeometry(NumMwmId mwmId)
{
  return GetIndexGraph(mwmId).GetGeometry();
}

void TestIndexGraphLoader::Clear() { m_graphs.clear(); }

void TestIndexGraphLoader::AddGraph(NumMwmId mwmId, unique_ptr<IndexGraph> graph)
{
  routing_test::AddGraph(m_graphs, mwmId, std::move(graph));
}

// TestTransitGraphLoader ----------------------------------------------------------------------------
TransitGraph & TestTransitGraphLoader::GetTransitGraph(NumMwmId mwmId, IndexGraph &)
{
  return GetGraph(m_graphs, mwmId);
}

void TestTransitGraphLoader::Clear() { m_graphs.clear(); }

void TestTransitGraphLoader::AddGraph(NumMwmId mwmId, unique_ptr<TransitGraph> graph)
{
  routing_test::AddGraph(m_graphs, mwmId, std::move(graph));
}

// WeightedEdgeEstimator --------------------------------------------------------------
double WeightedEdgeEstimator::CalcSegmentWeight(Segment const & segment,
                                                RoadGeometry const & /* road */,
                                                EdgeEstimator::Purpose /* purpose */) const
{
  auto const it = m_segmentWeights.find(segment);
  CHECK(it != m_segmentWeights.cend(), ());
  return it->second;
}

double WeightedEdgeEstimator::GetUTurnPenalty(Purpose purpose) const { return 0.0; }
double WeightedEdgeEstimator::GetFerryLandingPenalty(Purpose purpose) const { return 0.0; }

// TestIndexGraphTopology --------------------------------------------------------------------------
TestIndexGraphTopology::TestIndexGraphTopology(uint32_t numVertices) : m_numVertices(numVertices) {}

void TestIndexGraphTopology::AddDirectedEdge(Vertex from, Vertex to, double weight)
{
  AddDirectedEdge(m_edgeRequests, from, to, weight);
}

void TestIndexGraphTopology::SetEdgeAccess(Vertex from, Vertex to, RoadAccess::Type type)
{
  for (auto & r : m_edgeRequests)
  {
    if (r.m_from == from && r.m_to == to)
    {
      r.m_accessType = type;
      return;
    }
  }
  CHECK(false, ("Cannot set access for edge that is not in the graph", from, to));
}

void TestIndexGraphTopology::SetEdgeAccessConditional(Vertex from, Vertex to, RoadAccess::Type type,
                                                      string const & condition)
{
  for (auto & r : m_edgeRequests)
  {
    if (r.m_from == from && r.m_to == to)
    {
      osmoh::OpeningHours openingHours(condition);
      CHECK(openingHours.IsValid(), (condition));
      r.m_accessConditionalType.Insert(type, std::move(openingHours));
      return;
    }
  }
  CHECK(false, ("Cannot set access for edge that is not in the graph", from, to));
}

void TestIndexGraphTopology::SetVertexAccess(Vertex v, RoadAccess::Type type)
{
  bool found = false;
  for (auto & r : m_edgeRequests)
  {
    if (r.m_from == v)
    {
      r.m_fromAccessType = type;
      found = true;
    }
    else if (r.m_to == v)
    {
      r.m_toAccessType = type;
      found = true;
    }

    if (found)
      break;
  }
  CHECK(found, ("Cannot set access for vertex that is not in the graph", v));
}

void TestIndexGraphTopology::SetVertexAccessConditional(Vertex v, RoadAccess::Type type,
                                                        string const & condition)
{
  osmoh::OpeningHours openingHours(condition);
  CHECK(openingHours.IsValid(), (condition));

  bool found = false;
  for (auto & r : m_edgeRequests)
  {
    if (r.m_from == v)
    {
      r.m_fromAccessConditionalType.Insert(type, std::move(openingHours));
      found = true;
    }
    else if (r.m_to == v)
    {
      r.m_toAccessConditionalType.Insert(type, std::move(openingHours));
      found = true;
    }

    if (found)
      break;
  }
  CHECK(found, ("Cannot set access for vertex that is not in the graph", v));
}

bool TestIndexGraphTopology::FindPath(Vertex start, Vertex finish, double & pathWeight,
                                      vector<Edge> & pathEdges) const
{
  CHECK_LESS(start, m_numVertices, ());
  CHECK_LESS(finish, m_numVertices, ());

  if (start == finish)
  {
    pathWeight = 0.0;
    pathEdges.clear();
    return true;
  }

  auto edgeRequests = m_edgeRequests;
  // Edges of the index graph are segments, so we need a loop at finish
  // for the end of our path and another loop at start for the bidirectional search.
  auto const startFeatureId = static_cast<uint32_t>(edgeRequests.size());
  AddDirectedEdge(edgeRequests, start, start, 0.0);
  // |startSegment| corresponds to edge from |start| to |start| which has featureId |startFeatureId|
  // and the only segment with segmentIdx |0|. It is a loop so direction does not matter.
  auto const startSegment = Segment(kTestNumMwmId, startFeatureId, 0 /* segmentIdx */,
                                    true /* forward */);

  auto const finishFeatureId = static_cast<uint32_t>(edgeRequests.size());
  AddDirectedEdge(edgeRequests, finish, finish, 0.0);
  // |finishSegment| corresponds to edge from |finish| to |finish| which has featureId |finishFeatureId|
  // and the only segment with segmentIdx |0|. It is a loop so direction does not matter.
  auto const finishSegment = Segment(kTestNumMwmId, finishFeatureId, 0 /* segmentIdx */,
                                     true /* forward */);

  Builder builder(m_numVertices);
  builder.SetCurrentTimeGetter(m_currentTimeGetter);
  builder.BuildGraphFromRequests(edgeRequests);
  auto worldGraph = builder.PrepareIndexGraph();
  CHECK(worldGraph != nullptr, ());

  AlgorithmForWorldGraph algorithm;

  WorldGraphForAStar graphForAStar(std::move(worldGraph));

  AlgorithmForWorldGraph::ParamsForTests<> params(graphForAStar, startSegment, finishSegment);
  RoutingResult<Segment, RouteWeight> routingResult;
  auto const resultCode = algorithm.FindPathBidirectional(params, routingResult);

  // Check unidirectional AStar returns same result.
  {
    RoutingResult<Segment, RouteWeight> unidirectionalRoutingResult;
    auto const unidirectionalResultCode = algorithm.FindPath(params, unidirectionalRoutingResult);
    CHECK_EQUAL(resultCode, unidirectionalResultCode, ());
    CHECK(routingResult.m_distance.IsAlmostEqualForTests(unidirectionalRoutingResult.m_distance,
                                                         kEpsilon),
          ("Distances differ:", routingResult.m_distance, unidirectionalRoutingResult.m_distance));
  }

  if (resultCode == AlgorithmForWorldGraph::Result::NoPath)
    return false;

  CHECK_EQUAL(resultCode, AlgorithmForWorldGraph::Result::OK, ());
  CHECK_GREATER_OR_EQUAL(routingResult.m_path.size(), 2, ());
  CHECK_EQUAL(routingResult.m_path.front(), startSegment, ());
  CHECK_EQUAL(routingResult.m_path.back(), finishSegment, ());

  pathEdges.reserve(routingResult.m_path.size());
  pathWeight = 0.0;
  for (auto const & s : routingResult.m_path)
  {
    auto const it = builder.m_segmentToEdge.find(s);
    CHECK(it != builder.m_segmentToEdge.cend(), ());
    auto const & edge = it->second;
    pathEdges.push_back(edge);
    pathWeight += builder.m_segmentWeights[s];
  }

  // The loops from start to start and from finish to finish.
  CHECK_GREATER_OR_EQUAL(pathEdges.size(), 2, ());
  CHECK_EQUAL(pathEdges.front().first, pathEdges.front().second, ());
  CHECK_EQUAL(pathEdges.back().first, pathEdges.back().second, ());
  pathEdges.erase(pathEdges.begin());
  pathEdges.pop_back();

  return true;
}

void TestIndexGraphTopology::AddDirectedEdge(vector<EdgeRequest> & edgeRequests, Vertex from,
                                             Vertex to, double weight) const
{
  uint32_t const id = static_cast<uint32_t>(edgeRequests.size());
  edgeRequests.emplace_back(id, from, to, weight);
}

// TestIndexGraphTopology::Builder -----------------------------------------------------------------
unique_ptr<SingleVehicleWorldGraph> TestIndexGraphTopology::Builder::PrepareIndexGraph()
{
  auto loader = make_unique<ZeroGeometryLoader>();
  auto estimator = make_shared<WeightedEdgeEstimator>(m_segmentWeights);

  BuildJoints();

  auto worldGraph = BuildWorldGraph(std::move(loader), estimator, m_joints);
  auto & indexGraph = worldGraph->GetIndexGraphForTests(kTestNumMwmId);
  if (m_currentTimeGetter)
    indexGraph.SetCurrentTimeGetter(m_currentTimeGetter);
  indexGraph.SetRoadAccess(std::move(m_roadAccess));
  return worldGraph;
}

void TestIndexGraphTopology::Builder::BuildJoints()
{
  m_joints.resize(m_numVertices);
  for (uint32_t i = 0; i < m_joints.size(); ++i)
  {
    auto & joint = m_joints[i];

    for (auto const & segment : m_outgoingSegments[i])
      joint.AddPoint(RoadPoint(segment.GetFeatureId(), segment.GetPointId(false /* front */)));

    for (auto const & segment : m_ingoingSegments[i])
      joint.AddPoint(RoadPoint(segment.GetFeatureId(), segment.GetPointId(true /* front */)));
  }
}

void TestIndexGraphTopology::Builder::BuildGraphFromRequests(vector<EdgeRequest> const & requests)
{
  RoadAccess::WayToAccess wayToAccess;
  RoadAccess::WayToAccessConditional wayToAccessConditional;
  RoadAccess::PointToAccess pointToAccess;
  RoadAccess::PointToAccessConditional pointToAccessConditional;
  for (auto const & request : requests)
  {
    BuildSegmentFromEdge(request);
    if (request.m_accessType != RoadAccess::Type::Yes)
      wayToAccess[request.m_id] = request.m_accessType;

    if (!request.m_accessConditionalType.IsEmpty())
      wayToAccessConditional[request.m_id] = request.m_accessConditionalType;

    // All features have 1 segment. |from| has point index 0, |to| has point index 1.
    if (request.m_fromAccessType != RoadAccess::Type::Yes)
      pointToAccess[RoadPoint(request.m_id, 0 /* pointId */)] = request.m_fromAccessType;

    if (!request.m_fromAccessConditionalType.IsEmpty())
    {
      pointToAccessConditional[RoadPoint(request.m_id, 0 /* pointId */)] =
          request.m_fromAccessConditionalType;
    }

    if (request.m_toAccessType != RoadAccess::Type::Yes)
      pointToAccess[RoadPoint(request.m_id, 1 /* pointId */)] = request.m_toAccessType;

    if (!request.m_toAccessConditionalType.IsEmpty())
    {
      pointToAccessConditional[RoadPoint(request.m_id, 1 /* pointId */)] =
          request.m_toAccessConditionalType;
    }
  }

  m_roadAccess.SetAccess(std::move(wayToAccess), std::move(pointToAccess));
  m_roadAccess.SetAccessConditional(std::move(wayToAccessConditional), std::move(pointToAccessConditional));
  if (m_currentTimeGetter)
    m_roadAccess.SetCurrentTimeGetter(m_currentTimeGetter);
}

void TestIndexGraphTopology::Builder::BuildSegmentFromEdge(EdgeRequest const & request)
{
  auto const edge = make_pair(request.m_from, request.m_to);
  auto p = m_edgeWeights.emplace(edge, request.m_weight);
  CHECK(p.second, ("Multi-edges are not allowed"));

  uint32_t const featureId = request.m_id;
  Segment const segment(kTestNumMwmId, featureId, 0 /* segmentIdx */, true /* forward */);

  m_segmentWeights[segment] = request.m_weight;
  m_segmentToEdge[segment] = edge;
  m_outgoingSegments[request.m_from].push_back(segment);
  m_ingoingSegments[request.m_to].push_back(segment);
}

// Functions ---------------------------------------------------------------------------------------
unique_ptr<SingleVehicleWorldGraph> BuildWorldGraph(unique_ptr<TestGeometryLoader> geometryLoader,
                                                    shared_ptr<EdgeEstimator> estimator,
                                                    vector<Joint> const & joints)
{
  auto graph = make_unique<IndexGraph>(make_shared<Geometry>(std::move(geometryLoader)), estimator);
  graph->Import(joints);
  auto indexLoader = make_unique<TestIndexGraphLoader>();
  indexLoader->AddGraph(kTestNumMwmId, std::move(graph));
  return make_unique<SingleVehicleWorldGraph>(nullptr /* crossMwmGraph */, std::move(indexLoader),
                                              estimator, MwmHierarchyHandler());
}

unique_ptr<IndexGraph> BuildIndexGraph(unique_ptr<TestGeometryLoader> geometryLoader,
                                       shared_ptr<EdgeEstimator> estimator,
                                       vector<Joint> const & joints)
{
  auto graph = make_unique<IndexGraph>(make_shared<Geometry>(std::move(geometryLoader)), estimator);
  graph->Import(joints);
  return graph;
}

unique_ptr<SingleVehicleWorldGraph> BuildWorldGraph(unique_ptr<ZeroGeometryLoader> geometryLoader,
                                                    shared_ptr<EdgeEstimator> estimator,
                                                    vector<Joint> const & joints)
{
  auto graph = make_unique<IndexGraph>(make_shared<Geometry>(std::move(geometryLoader)), estimator);

  graph->Import(joints);
  auto indexLoader = make_unique<TestIndexGraphLoader>();
  indexLoader->AddGraph(kTestNumMwmId, std::move(graph));
  return make_unique<SingleVehicleWorldGraph>(nullptr /* crossMwmGraph */, std::move(indexLoader),
                                              estimator, MwmHierarchyHandler());
}

unique_ptr<TransitWorldGraph> BuildWorldGraph(unique_ptr<TestGeometryLoader> geometryLoader,
                                              shared_ptr<EdgeEstimator> estimator,
                                              vector<Joint> const & joints,
                                              routing::transit::GraphData const & transitData)
{
  auto indexGraph = make_unique<IndexGraph>(make_shared<Geometry>(std::move(geometryLoader)), estimator);
  indexGraph->Import(joints);

  auto transitGraph =
      make_unique<TransitGraph>(::transit::TransitVersion::OnlySubway, kTestNumMwmId, estimator);
  TransitGraph::Endings gateEndings;
  MakeGateEndings(transitData.GetGates(), kTestNumMwmId, *indexGraph, gateEndings);
  transitGraph->Fill(transitData, gateEndings);

  auto indexLoader = make_unique<TestIndexGraphLoader>();
  indexLoader->AddGraph(kTestNumMwmId, std::move(indexGraph));

  auto transitLoader = make_unique<TestTransitGraphLoader>();
  transitLoader->AddGraph(kTestNumMwmId, std::move(transitGraph));

  return make_unique<TransitWorldGraph>(nullptr /* crossMwmGraph */, std::move(indexLoader),
                                        std::move(transitLoader), estimator);
}

AlgorithmForWorldGraph::Result CalculateRoute(IndexGraphStarter & starter, vector<Segment> & roadPoints,
                                              double & timeSec)
{
  AlgorithmForWorldGraph algorithm;
  RoutingResult<Segment, RouteWeight> routingResult;

  AlgorithmForWorldGraph::ParamsForTests<AStarLengthChecker> params(
      starter, starter.GetStartSegment(), starter.GetFinishSegment(), AStarLengthChecker(starter));

  auto const resultCode = algorithm.FindPathBidirectional(params, routingResult);

  timeSec = routingResult.m_distance.GetWeight();
  roadPoints = routingResult.m_path;
  return resultCode;
}

void TestRouteGeometry(IndexGraphStarter & starter,
                       AlgorithmForWorldGraph::Result expectedRouteResult,
                       vector<m2::PointD> const & expectedRouteGeom)
{
  vector<Segment> routeSegs;
  double timeSec = 0.0;
  auto const resultCode = CalculateRoute(starter, routeSegs, timeSec);

  TEST_EQUAL(resultCode, expectedRouteResult, ());

  if (AlgorithmForWorldGraph::Result::NoPath == expectedRouteResult &&
      expectedRouteGeom.empty())
  {
    // The route goes through a restriction. So there's no choice for building route
    // except for going through restriction. So no path.
    return;
  }

  if (resultCode != AlgorithmForWorldGraph::Result::OK)
    return;

  CHECK(!routeSegs.empty(), ());
  vector<m2::PointD> geom;

  auto const pushPoint = [&geom](ms::LatLon const & ll) {
    auto const point = mercator::FromLatLon(ll);
    if (geom.empty() || geom.back() != point)
      geom.push_back(point);
  };

  for (auto const & routeSeg : routeSegs)
  {
    auto const & ll = starter.GetPoint(routeSeg, false /* front */);
    // Note. In case of A* router all internal points of route are duplicated.
    // So it's necessary to exclude the duplicates.
    pushPoint(ll);
  }

  pushPoint(starter.GetPoint(routeSegs.back(), false /* front */));
  TEST_EQUAL(geom.size(), expectedRouteGeom.size(), ("geom:", geom, "expectedRouteGeom:", expectedRouteGeom));
  for (size_t i = 0; i < geom.size(); ++i)
  {
    static double constexpr kEps = 1e-8;
    if (!AlmostEqualAbs(geom[i], expectedRouteGeom[i], kEps))
    {
      for (size_t j = 0; j < geom.size(); ++j)
        LOG(LINFO, (j, "=>", geom[j], "vs", expectedRouteGeom[j]));

      TEST(false, ("Point with number:", i, "doesn't equal to expected."));
    }
  }
}

void TestRestrictions(vector<m2::PointD> const & expectedRouteGeom,
                      AlgorithmForWorldGraph::Result expectedRouteResult,
                      FakeEnding const & start, FakeEnding const & finish,
                      RestrictionVec && restrictions,
                      RestrictionTest & restrictionTest)
{
  restrictionTest.SetRestrictions(std::move(restrictions));
  restrictionTest.SetStarter(start, finish);

  TestRouteGeometry(*restrictionTest.m_starter, expectedRouteResult, expectedRouteGeom);
}

void TestRestrictions(vector<m2::PointD> const & expectedRouteGeom,
                      AlgorithmForWorldGraph::Result expectedRouteResult,
                      FakeEnding const & start, FakeEnding const & finish,
                      RestrictionVec && restrictions,
                      vector<RestrictionUTurn> && restrictionsNoUTurn,
                      RestrictionTest & restrictionTest)
{
  restrictionTest.SetRestrictions(std::move(restrictions));
  restrictionTest.SetUTurnRestrictions(std::move(restrictionsNoUTurn));

  restrictionTest.SetStarter(start, finish);
  TestRouteGeometry(*restrictionTest.m_starter, expectedRouteResult, expectedRouteGeom);
}

void TestRestrictions(double expectedLength,
                      FakeEnding const & start, FakeEnding const & finish,
                      RestrictionVec && restrictions, RestrictionTest & restrictionTest)
{
  restrictionTest.SetRestrictions(std::move(restrictions));
  restrictionTest.SetStarter(start, finish);

  auto & starter = *restrictionTest.m_starter;

  double timeSec = 0.0;
  vector<Segment> segments;
  auto const resultCode = CalculateRoute(starter, segments, timeSec);
  TEST_EQUAL(resultCode, AlgorithmForWorldGraph::Result::OK, ());

  double length = 0.0;
  for (auto const & segment : segments)
  {
    auto const back = mercator::FromLatLon(starter.GetPoint(segment, false /* front */));
    auto const front = mercator::FromLatLon(starter.GetPoint(segment, true /* front */));

    length += back.Length(front);
  }

  static auto constexpr kEps = 1e-3;
  TEST(AlmostEqualAbs(expectedLength, length, kEps),
       ("Length expected:", expectedLength, "has:", length));
}

void TestTopologyGraph(TestIndexGraphTopology const & graph, TestIndexGraphTopology::Vertex from,
                       TestIndexGraphTopology::Vertex to, bool expectedPathFound,
                       double const expectedWeight,
                       vector<TestIndexGraphTopology::Edge> const & expectedEdges)
{
  double pathWeight = 0.0;
  vector<TestIndexGraphTopology::Edge> pathEdges;
  bool const pathFound = graph.FindPath(from, to, pathWeight, pathEdges);
  TEST_EQUAL(pathFound, expectedPathFound, ());
  if (!pathFound)
    return;

  TEST(AlmostEqualAbs(pathWeight, expectedWeight, kEpsilon),
       (pathWeight, expectedWeight, pathEdges));
  TEST_EQUAL(pathEdges, expectedEdges, ());
}

FakeEnding MakeFakeEnding(uint32_t featureId, uint32_t segmentIdx, m2::PointD const & point,
                          WorldGraph & graph)
{
  return MakeFakeEnding({Segment(kTestNumMwmId, featureId, segmentIdx, true /* forward */)}, point,
                        graph);
}
unique_ptr<IndexGraphStarter> MakeStarter(FakeEnding const & start, FakeEnding const & finish,
                                          WorldGraph & graph)
{
  return make_unique<IndexGraphStarter>(start, finish, 0 /* fakeNumerationStart */,
                                        false /* strictForward */, graph);
}

time_t GetUnixtimeByDate(uint16_t year, Month month, uint8_t monthDay, uint8_t hours,
                         uint8_t minutes)
{
  std::tm t{};
  t.tm_year = year - 1900;
  t.tm_mon = static_cast<int>(month) - 1;
  t.tm_mday = monthDay;
  t.tm_hour = hours;
  t.tm_min = minutes;

  time_t moment = mktime(&t);
  return moment;
}

time_t GetUnixtimeByDate(uint16_t year, Month month, Weekday weekday, uint8_t hours,
                         uint8_t minutes)
{
  int monthDay = 1;
  auto createUnixtime = [&]() {
    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = static_cast<int>(month) - 1;
    t.tm_mday = monthDay;
    t.tm_wday = static_cast<int>(weekday) - 1;
    t.tm_hour = hours;
    t.tm_min = minutes;

    return mktime(&t);
  };

  int wday = -1;
  for (;;)
  {
    auto unixtime = createUnixtime();
    auto timeOut = localtime(&unixtime);
    wday = timeOut->tm_wday;
    if (wday == static_cast<int>(weekday) - 1)
      break;
    ++monthDay;
  }

  return createUnixtime();
}
}  // namespace routing_test
