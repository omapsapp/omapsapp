#include "generator/routing_index_generator.hpp"

#include "generator/borders.hpp"
#include "generator/cross_mwm_osm_ways_collector.hpp"
#include "generator/routing_helpers.hpp"

#include "routing/base/astar_algorithm.hpp"
#include "routing/base/astar_graph.hpp"
#include "routing/cross_mwm_connector.hpp"
#include "routing/cross_mwm_connector_serialization.hpp"
#include "routing/cross_mwm_ids.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_loader.hpp"
#include "routing/index_graph_serialization.hpp"
#include "routing/index_graph_starter_joints.hpp"
#include "routing/joint_segment.hpp"
#include "routing/vehicle_mask.hpp"
#include "routing/world_graph.hpp"

#include "transit/experimental/transit_data.hpp"
#include "transit/transit_graph_data.hpp"

#include "routing_common/bicycle_model.hpp"
#include "routing_common/car_model.hpp"
#include "routing_common/pedestrian_model.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_processor.hpp"

#include "coding/files_container.hpp"
#include "coding/point_coding.hpp"
#include "coding/reader.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/file_name_utils.hpp"
#include "base/geo_object_id.hpp"
#include "base/logging.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace routing_builder
{
using namespace feature;
using namespace platform;
using namespace routing;
using std::string, std::vector;

class VehicleMaskBuilder final
{
public:
  VehicleMaskBuilder(string const & country, CountryParentNameGetterFn const & countryParentNameGetterFn)
    : m_pedestrianModel(PedestrianModelFactory(countryParentNameGetterFn).GetVehicleModelForCountry(country))
    , m_bicycleModel(BicycleModelFactory(countryParentNameGetterFn).GetVehicleModelForCountry(country))
    , m_carModel(CarModelFactory(countryParentNameGetterFn).GetVehicleModelForCountry(country))
    , m_constructionType(classif().GetTypeByPath({"highway", "construction"}))
  {
    CHECK(m_pedestrianModel, ());
    CHECK(m_bicycleModel, ());
    CHECK(m_carModel, ());
  }

  VehicleMask CalcRoadMask(FeatureType & f) const
  {
    feature::TypesHolder const types(f);
    if (types.HasWithSubclass(m_constructionType))
      return 0;

    return CalcMask([&](VehicleModelInterface const & model)
    {
      return model.IsRoad(types);
    });
  }

  VehicleMask CalcOneWayMask(FeatureType & f) const
  {
    feature::TypesHolder const types(f);
    return CalcMask([&](VehicleModelInterface const & model)
    {
      return model.IsOneWay(types);
    });
  }

private:
  template <class Fn>
  VehicleMask CalcMask(Fn && fn) const
  {
    VehicleMask mask = 0;
    if (fn(*m_pedestrianModel))
      mask |= kPedestrianMask;
    if (fn(*m_bicycleModel))
      mask |= kBicycleMask;
    if (fn(*m_carModel))
      mask |= kCarMask;

    return mask;
  }

  std::shared_ptr<VehicleModelInterface> const m_pedestrianModel;
  std::shared_ptr<VehicleModelInterface> const m_bicycleModel;
  std::shared_ptr<VehicleModelInterface> const m_carModel;

  uint32_t const m_constructionType;
};

class Processor final
{
public:
  Processor(string const & country,
            CountryParentNameGetterFn const & countryParentNameGetterFn)
    : m_maskBuilder(country, countryParentNameGetterFn)
  {
  }

  void ProcessAllFeatures(string const & filename)
  {
    using namespace std::placeholders;
    ForEachFeature(filename, std::bind(&Processor::ProcessFeature, this, _1, _2));
  }

  void BuildGraph(IndexGraph & graph) const
  {
    vector<Joint> joints;
    for (auto const & it : m_posToJoint)
    {
      // Need only connected points (2 or more roads)
      if (it.second.GetSize() >= 2)
        joints.emplace_back(it.second);
    }

    graph.Import(joints);
  }

  std::unordered_map<uint32_t, VehicleMask> const & GetMasks() const { return m_masks; }

private:
  void ProcessFeature(FeatureType & f, uint32_t id)
  {
    VehicleMask const mask = m_maskBuilder.CalcRoadMask(f);
    if (mask == 0)
      return;

    m_masks[id] = mask;
    f.ParseGeometry(FeatureType::BEST_GEOMETRY);

    for (size_t i = 0; i < f.GetPointsCount(); ++i)
    {
      uint64_t const locationKey = PointToInt64Obsolete(f.GetPoint(i), kPointCoordBits);
      m_posToJoint[locationKey].AddPoint(RoadPoint(id, base::checked_cast<uint32_t>(i)));
    }
  }

  VehicleMaskBuilder const m_maskBuilder;
  std::unordered_map<uint64_t, Joint> m_posToJoint;
  std::unordered_map<uint32_t, VehicleMask> m_masks;
};

class IndexGraphWrapper final
{
public:
  IndexGraphWrapper(IndexGraph & graph, Segment const & start)
    : m_graph(graph), m_start(start)
  {
  }

  /// @name For compatibility with IndexGraphStarterJoints
  /// @{
  Segment GetStartSegment() const { return m_start; }
  Segment GetFinishSegment() const { return {}; }
  bool ConvertToReal(Segment const & /* segment */) const { return false; }
  RouteWeight HeuristicCostEstimate(Segment const & /* from */,
                                             ms::LatLon const & /* to */)
  {
    CHECK(false, ("This method exists only for compatibility with IndexGraphStarterJoints"));
    return GetAStarWeightZero<RouteWeight>();
  }

  bool AreWavesConnectible(
      IndexGraph::Parents<JointSegment> const & /* forwardParents */,
      JointSegment const & /* commonVertex */,
      IndexGraph::Parents<JointSegment> const & /* backwardParents */,
      WorldGraph::FakeConverterT const & /* fakeFeatureConverter */)
  {
    return true;
  }

  void SetAStarParents(bool /* forward */,
                       IndexGraph::Parents<JointSegment> & parents)
  {
    m_AStarParents = &parents;
  }

  void DropAStarParents()
  {
    m_AStarParents = nullptr;
  }

  RouteWeight GetAStarWeightEpsilon() { return RouteWeight(0.0); }

  RouteWeight GetCrossBorderPenalty(NumMwmId /* mwmId1 */, NumMwmId /* mwmId2 */) { return RouteWeight(0); }
  /// @}

  ms::LatLon const & GetPoint(Segment const & s, bool forward)
  {
    return m_graph.GetPoint(s, forward);
  }

  using SegmentEdgeListT = IndexGraph::SegmentEdgeListT;
  using EdgeListT = SegmentEdgeListT;
  void GetEdgesList(Segment const & child, bool isOutgoing,
                    SegmentEdgeListT & edges)
  {
    m_graph.GetEdgeList(child, isOutgoing, true /* useRoutingOptions */, edges);
  }

  using JointEdgeListT = IndexGraph::JointEdgeListT;
  using WeightListT = IndexGraph::WeightListT;

  void GetEdgeList(
      astar::VertexData<JointSegment, RouteWeight> const & vertexData,
      Segment const & parent, bool isOutgoing, JointEdgeListT & edges,
      WeightListT & parentWeights) const
  {
    CHECK(m_AStarParents, ());
    return m_graph.GetEdgeList(vertexData.m_vertex, parent, isOutgoing, edges, parentWeights,
                               *m_AStarParents);
  }

  bool IsJoint(Segment const & segment, bool fromStart) const
  {
    return IsJointOrEnd(segment, fromStart);
  }

  bool IsJointOrEnd(Segment const & segment, bool fromStart) const
  {
    return m_graph.IsJointOrEnd(segment, fromStart);
  }

  template <typename Vertex>
  RouteWeight HeuristicCostEstimate(Vertex const & /* from */, m2::PointD const & /* to */)
  {
    CHECK(false, ("This method should not be use, it is just for compatibility with "
                  "IndexGraphStarterJoints."));

    return GetAStarWeightZero<RouteWeight>();
  }

private:
  IndexGraph::Parents<JointSegment> * m_AStarParents = nullptr;
  IndexGraph & m_graph;
  Segment m_start;
};

class DijkstraWrapperJoints : public IndexGraphStarterJoints<IndexGraphWrapper>
{
public:
  DijkstraWrapperJoints(IndexGraphWrapper & graph, Segment const & start)
    : IndexGraphStarterJoints<IndexGraphWrapper>(graph, start)
  {
  }

  Weight HeuristicCostEstimate(Vertex const & /* from */, Vertex const & /* to */) override
  {
    return GetAStarWeightZero<Weight>();
  }
};

/// \brief Fills |transitions| for osm id case. That means |Transition::m_roadMask| for items in
/// |transitions| will be combination of |VehicleType::Pedestrian|, |VehicleType::Bicycle|
/// and |VehicleType::Car|.
void CalcCrossMwmTransitions(
    string const & mwmFile, string const & intermediateDir, string const & mappingFile,
    vector<m2::RegionD> const & /* borders */, string const & country,
    CountryParentNameGetterFn const & countryParentNameGetterFn,
    CrossMwmConnectorBuilderEx<base::GeoObjectId> & builder)
{
  VehicleMaskBuilder const maskMaker(country, countryParentNameGetterFn);
  std::map<uint32_t, base::GeoObjectId> featureIdToOsmId;
  ParseWaysFeatureIdToOsmIdMapping(mappingFile, featureIdToOsmId);

  auto const & path = base::JoinPath(intermediateDir, CROSS_MWM_OSM_WAYS_DIR, country);

  using CrossMwmInfoT = generator::CrossMwmOsmWaysCollector::CrossMwmInfo;
  auto const crossMwmOsmIdWays = CrossMwmInfoT::LoadFromFileToSet(path);

  ForEachFeature(mwmFile, [&](FeatureType & f, uint32_t featureId) {
    VehicleMask const roadMask = maskMaker.CalcRoadMask(f);
    if (roadMask == 0)
      return;

    auto const it = featureIdToOsmId.find(featureId);
    CHECK(it != featureIdToOsmId.end(), ("Can't find osm id for feature id", featureId));
    auto const osmId = it->second;
    CHECK(osmId.GetType() == base::GeoObjectId::Type::ObsoleteOsmWay, ());

    auto const crossMwmWayInfoIt = crossMwmOsmIdWays.find(CrossMwmInfoT(osmId.GetEncodedId()));

    if (crossMwmWayInfoIt != crossMwmOsmIdWays.cend())
    {
      f.ParseGeometry(FeatureType::BEST_GEOMETRY);

      VehicleMask const oneWayMask = maskMaker.CalcOneWayMask(f);
      for (auto const & seg : crossMwmWayInfoIt->m_crossMwmSegments)
        builder.AddTransition(osmId, featureId, seg.m_segmentId, roadMask, oneWayMask, seg.m_forwardIsEnter);
    }
  });
}

/// \brief Fills |transitions| for transit case. That means Transition::m_roadMask for items in
/// |transitions| will be equal to VehicleType::Transit after call of this method.
void CalcCrossMwmTransitions(
    string const & mwmFile, string const & intermediateDir, string const & mappingFile,
    vector<m2::RegionD> const & borders, string const & /* country */,
    CountryParentNameGetterFn const & /* countryParentNameGetterFn */,
    CrossMwmConnectorBuilderEx<connector::TransitId> & builder)
{
  CHECK(mappingFile.empty(), ());
  CHECK(intermediateDir.empty(), ());
  try
  {
    FilesContainerR cont(mwmFile);
    if (!cont.IsExist(TRANSIT_FILE_TAG))
    {
      LOG(LINFO, ("Transit cross mwm section is not generated because no transit section in mwm:",
                  mwmFile));
      return;
    }
    auto reader = cont.GetReader(TRANSIT_FILE_TAG);

    using namespace routing::transit;

    GraphData graphData;
    graphData.DeserializeForCrossMwm(*reader.GetPtr());
    auto const & stops = graphData.GetStops();
    auto const & edges = graphData.GetEdges();

    auto const getStopIdPoint = [&stops](StopId stopId) -> m2::PointD const & {
      auto const it = equal_range(stops.cbegin(), stops.cend(), Stop(stopId));
      CHECK_EQUAL(
          distance(it.first, it.second), 1,
          ("A stop with id:", stopId, "is not unique or there's no such item in stops:", stops));
      return it.first->GetPoint();
    };

    // Index |i| is a zero based edge index. This zero based index should be increased with
    // |FakeFeatureIds::kTransitGraphFeaturesStart| by setting it as |featureNumerationOffset| for
    // CrossMwmConnector (see CrossMwmIndexGraph::Deserialize()) and then used in Segment class as
    // feature id in transit case.
    for (size_t i = 0; i < edges.size(); ++i)
    {
      auto const & e = edges[i];
      m2::PointD const & stop1Point = getStopIdPoint(e.GetStop1Id());
      m2::PointD const & stop2Point = getStopIdPoint(e.GetStop2Id());
      bool const stop2In = m2::RegionsContain(borders, stop2Point);
      if (m2::RegionsContain(borders, stop1Point) == stop2In)
        continue;

      // Note. One way mask is set to kTransitMask because all transit edges are one way edges.
      builder.AddTransition(
          connector::TransitId(e.GetStop1Id(), e.GetStop2Id(), e.GetLineId()),
          i /* feature id */, 0 /* segment index */, kTransitMask,
          kTransitMask /* one way mask */, stop2In /* forward is enter */);
    }
  }
  catch (Reader::OpenException const & e)
  {
    CHECK(false, ("Error while reading", TRANSIT_FILE_TAG, "section.", e.Msg()));
  }
}

/// TODO(o.khlopkova) Rename CalcCrossMwmTransitionsExperimental() and remove
/// CalcCrossMwmTransitions() when we abandon support of "subway" transit section version.
/// \brief Fills |transitions| for experimental transit case. It means that Transition::m_roadMask
/// for items in |transitions| will be equal to VehicleType::Transit after the call of this method.
void CalcCrossMwmTransitionsExperimental(
    string const & mwmFile, vector<m2::RegionD> const & borders, string const & /* country */,
    CountryParentNameGetterFn const & /* countryParentNameGetterFn */,
    ::transit::experimental::EdgeIdToFeatureId const & edgeIdToFeatureId,
    CrossMwmConnectorBuilderEx<connector::TransitId> & builder)
{
  try
  {
    FilesContainerR cont(mwmFile);
    if (!cont.IsExist(TRANSIT_FILE_TAG))
    {
      LOG(LINFO, ("Experimental transit cross mwm section is not generated because there is no "
                  "experimental transit section in mwm:",
                  mwmFile));
      return;
    }
    auto reader = cont.GetReader(TRANSIT_FILE_TAG);

    ::transit::experimental::TransitData transitData;
    transitData.DeserializeForCrossMwm(*reader.GetPtr());
    auto const & stops = transitData.GetStops();
    auto const & edges = transitData.GetEdges();

    auto const getStopIdPoint = [&stops](::transit::TransitId stopId) {
      auto const it = find_if(
          stops.begin(), stops.end(),
          [stopId](::transit::experimental::Stop const & stop) { return stop.GetId() == stopId; });

      CHECK(it != stops.end(),
            ("stopId:", stopId, "is not found in stops. Size of stops:", stops.size()));
      return it->GetPoint();
    };

    // Index |i| is a zero based edge index. This zero based index should be increased with
    // |FakeFeatureIds::kTransitGraphFeaturesStart| by calling CrossMwmConnectorBuilder::ApplyNumerationOffset.
    for (auto const & e : edges)
    {
      m2::PointD const & stop1Point = getStopIdPoint(e.GetStop1Id());
      m2::PointD const & stop2Point = getStopIdPoint(e.GetStop2Id());
      bool const stop2In = m2::RegionsContain(borders, stop2Point);
      if (m2::RegionsContain(borders, stop1Point) == stop2In)
        continue;

      auto const it =
          edgeIdToFeatureId.find(::transit::EdgeId(e.GetStop1Id(), e.GetStop2Id(), e.GetLineId()));
      CHECK(it != edgeIdToFeatureId.end(),
            ("Each edge in transitData corresponds to the edgeIdToFeatureId key."));

      uint32_t const featureId = it->second;
      // Note. One way mask is set to kTransitMask because all transit edges are one way edges.
      builder.AddTransition(
          connector::TransitId(e.GetStop1Id(), e.GetStop2Id(), e.GetLineId()),
          featureId /* feature id */, 0 /* segment index */, kTransitMask,
          kTransitMask /* one way mask */, stop2In /* forward is enter */);
    }
  }
  catch (Reader::OpenException const & e)
  {
    CHECK(false, ("Error while reading", TRANSIT_FILE_TAG, "section.", e.Msg()));
  }
}

// Dummy specialization. We need it to compile this function overload for experimental transit.
void CalcCrossMwmTransitionsExperimental(
    string const & /* mwmFile */, vector<m2::RegionD> const & /* borders */, string const & /* country */,
    CountryParentNameGetterFn const & /* countryParentNameGetterFn */,
    ::transit::experimental::EdgeIdToFeatureId const & /* edgeIdToFeatureId */,
    CrossMwmConnectorBuilderEx<base::GeoObjectId> & /* builder */)
{
  CHECK(false, ("This is dummy specialization and it shouldn't be called."));
}

/// \brief Fills |transitions| and |connectors| params.
/// \note This method fills only |connections| which are applicable for |CrossMwmId|.
/// For example |VehicleType::Pedestrian|, |VehicleType::Bicycle| and |VehicleType::Car|
/// are applicable for |connector::OsmId|.
/// And |VehicleType::Transit| is applicable for |connector::TransitId|.
template <typename CrossMwmId>
void CalcCrossMwmConnectors(
    string const & path, string const & mwmFile, string const & intermediateDir,
    string const & country, CountryParentNameGetterFn const & countryParentNameGetterFn,
    string const & mappingFile,
    ::transit::experimental::EdgeIdToFeatureId const & edgeIdToFeatureId,
    CrossMwmConnectorBuilderEx<CrossMwmId> & builder,
    bool experimentalTransit = false)
{
  base::Timer timer;
  string const polyFile = base::JoinPath(path, BORDERS_DIR, country + BORDERS_EXTENSION);
  vector<m2::RegionD> borders;
  borders::LoadBorders(polyFile, borders);

  // Note 1. CalcCrossMwmTransitions() method fills vector |transitions|.
  // There are two implementation of the method for |connector::OsmId| and for |connector::TransitId|.
  // For all items in |transitions| |Transition::m_roadMask| may be set to any combination of masks:
  // GetVehicleMask(VehicleType::Pedestrian), GetVehicleMask(VehicleType::Bicycle) and
  // GetVehicleMask(VehicleType::Car) for |connector::OsmId| implementation.
  // For all items in |transitions| |Transition::m_roadMask| is set to
  // GetVehicleMask(VehicleType::Transit) for |connector::TransitId| implementation.
  // Note 2. Taking into account note 1 it's clear that field |Transition<TransitId>::m_roadMask|
  // is always set to |VehicleType::Transit| and field |Transition<OsmId>::m_roadMask| can't have
  // |VehicleType::Transit| value.
  if (experimentalTransit)
  {
    CHECK(!edgeIdToFeatureId.empty(),
          ("Edge id to feature id must be filled before building cross-mwm transit section."));
    CalcCrossMwmTransitionsExperimental(mwmFile, borders, country, countryParentNameGetterFn,
                                        edgeIdToFeatureId, builder);
  }
  else
  {
    CHECK(edgeIdToFeatureId.empty(),
          ("Edge id to feature id must not be filled for subway vesion of transit section."));
    CalcCrossMwmTransitions(mwmFile, intermediateDir, mappingFile, borders, country,
                            countryParentNameGetterFn, builder);
  }

  LOG(LINFO, ("Transitions count =", builder.GetTransitionsCount(), "elapsed:", timer.ElapsedSeconds(), "seconds"));
}

template <typename CrossMwmId>
void FillWeights(string const & path, string const & mwmFile, string const & country,
                 CountryParentNameGetterFn const & countryParentNameGetterFn,
                 CrossMwmConnectorBuilderEx<CrossMwmId> & builder)
{
  base::Timer timer;

  // We use leaps for cars only. To use leaps for other vehicle types add weights generation
  // here and change WorldGraph mode selection rule in IndexRouter::CalculateSubroute.
  VehicleType const vhType = VehicleType::Car;
  std::shared_ptr<VehicleModelInterface> vehicleModel =
      CarModelFactory(countryParentNameGetterFn).GetVehicleModelForCountry(country);

  MwmValue mwmValue(LocalCountryFile(path, platform::CountryFile(country), 0 /* version */));
  uint32_t mwmNumRoads = DeserializeIndexGraphNumRoads(mwmValue, vhType);
  IndexGraph graph(std::make_shared<Geometry>(GeometryLoader::CreateFromFile(mwmFile, vehicleModel), mwmNumRoads),
                                              EdgeEstimator::Create(vhType, *vehicleModel,
                                                                    nullptr /* trafficStash */,
                                                                    nullptr /* dataSource */,
                                                                    nullptr /* numMvmIds */));
  graph.SetCurrentTimeGetter([time = GetCurrentTimestamp()] { return time; });
  DeserializeIndexGraph(mwmValue, vhType, graph);

  std::map<Segment, std::map<Segment, RouteWeight>> weights;
  size_t foundCount = 0;
  size_t notFoundCount = 0;

  auto const & connector = builder.PrepareConnector(vhType);
  uint32_t const numEnters = connector.GetNumEnters();
  uint32_t i = 0;
  connector.ForEachEnter([&](uint32_t, Segment const & enter)
  {
    if (i % 10 == 0)
      LOG(LINFO, ("Building leaps:", i, "/", numEnters, "waves passed"));

    i++;
    using Algorithm =
        AStarAlgorithm<JointSegment, JointEdge, RouteWeight>;

    Algorithm astar;
    IndexGraphWrapper indexGraphWrapper(graph, enter);
    DijkstraWrapperJoints wrapper(indexGraphWrapper, enter);
    Algorithm::Context context(wrapper);

    std::unordered_map<uint32_t, vector<JointSegment>> visitedVertexes;
    astar.PropagateWave(
        wrapper, wrapper.GetStartJoint(),
        [&](JointSegment const & vertex)
        {
          if (vertex.IsFake())
          {
            auto const & start = wrapper.GetSegmentOfFakeJoint(vertex, true /* start */);
            auto const & end = wrapper.GetSegmentOfFakeJoint(vertex, false /* start */);
            if (start.IsForward() != end.IsForward())
              return true;

            visitedVertexes[end.GetFeatureId()].emplace_back(start, end);
          }
          else
          {
            visitedVertexes[vertex.GetFeatureId()].emplace_back(vertex);
          }

          return true;
        } /* visitVertex */,
        context);

    connector.ForEachExit([&](uint32_t, Segment const & exit)
    {
      auto const it = visitedVertexes.find(exit.GetFeatureId());
      if (it == visitedVertexes.cend())
      {
        ++notFoundCount;
        return;
      }

      uint32_t const id = exit.GetSegmentIdx();
      bool const forward = exit.IsForward();
      for (auto const & jointSegment : it->second)
      {
        if (jointSegment.IsForward() != forward)
          continue;

        if ((jointSegment.GetStartSegmentId() <= id && id <= jointSegment.GetEndSegmentId()) ||
            (jointSegment.GetEndSegmentId() <= id && id <= jointSegment.GetStartSegmentId()))
        {
          RouteWeight weight;
          Segment parentSegment;
          if (context.HasParent(jointSegment))
          {
            JointSegment const & parent = context.GetParent(jointSegment);
            parentSegment = wrapper.GetSegmentFromJoint(parent, false /* start */);
            weight = context.GetDistance(parent);
          }
          else
          {
            parentSegment = enter;
          }

          Segment const & firstChild = jointSegment.GetSegment(true /* start */);
          uint32_t const lastPoint = exit.GetPointId(true /* front */);

          auto optionalEdge =  graph.GetJointEdgeByLastPoint(parentSegment, firstChild,
                                                             true /* isOutgoing */, lastPoint);

          if (!optionalEdge)
            continue;

          weight += (*optionalEdge).GetWeight();
          weights[enter][exit] = weight;

          ++foundCount;
          break;
        }
      }
    });
  });

  builder.FillWeights([&](Segment const & enter, Segment const & exit) {
    auto it0 = weights.find(enter);
    if (it0 == weights.end())
      return connector::kNoRoute;

    auto it1 = it0->second.find(exit);
    if (it1 == it0->second.end())
      return connector::kNoRoute;

    return it1->second.ToCrossMwmWeight();
  });

  LOG(LINFO, ("Leaps finished, elapsed:", timer.ElapsedSeconds(), "seconds, routes found:",
              foundCount, ", not found:", notFoundCount));
}

bool BuildRoutingIndex(string const & filename, string const & country,
                       CountryParentNameGetterFn const & countryParentNameGetterFn)
{
  LOG(LINFO, ("Building routing index for", filename));
  try
  {
    Processor processor(country, countryParentNameGetterFn);
    processor.ProcessAllFeatures(filename);

    IndexGraph graph;
    processor.BuildGraph(graph);

    FilesContainerW cont(filename, FileWriter::OP_WRITE_EXISTING);
    auto writer = cont.GetWriter(ROUTING_FILE_TAG);

    auto const startPos = writer->Pos();
    IndexGraphSerializer::Serialize(graph, processor.GetMasks(), *writer);
    auto const sectionSize = writer->Pos() - startPos;

    LOG(LINFO, ("Routing section created:", sectionSize, "bytes,", graph.GetNumRoads(), "roads,",
                graph.GetNumJoints(), "joints,", graph.GetNumPoints(), "points"));
    return true;
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("An exception happened while creating", ROUTING_FILE_TAG, "section:", e.what()));
    return false;
  }
}

/// \brief Serializes all the cross mwm information to |sectionName| of |mwmFile| including:
/// * header
/// * transitions
/// * weight buffers if any
template <typename CrossMwmId>
void SerializeCrossMwm(string const & mwmFile, string const & sectionName,
                       CrossMwmConnectorBuilderEx<CrossMwmId> & builder)
{
  FilesContainerW cont(mwmFile, FileWriter::OP_WRITE_EXISTING);
  auto writer = cont.GetWriter(sectionName);
  auto const startPos = writer->Pos();
  builder.Serialize(*writer);
  auto const sectionSize = writer->Pos() - startPos;

  LOG(LINFO, ("Cross mwm section generated, size:", sectionSize, "bytes"));
}

void BuildRoutingCrossMwmSection(string const & path, string const & mwmFile,
                                 string const & country, string const & intermediateDir,
                                 CountryParentNameGetterFn const & countryParentNameGetterFn,
                                 string const & osmToFeatureFile)
{
  LOG(LINFO, ("Building cross mwm section for", country));
  CrossMwmConnectorBuilderEx<base::GeoObjectId> builder;

  CalcCrossMwmConnectors(path, mwmFile, intermediateDir, country, countryParentNameGetterFn,
                         osmToFeatureFile, {} /* edgeIdToFeatureId */, builder);

  // We use leaps for cars only. To use leaps for other vehicle types add weights generation
  // here and change WorldGraph mode selection rule in IndexRouter::CalculateSubroute.
  FillWeights(path, mwmFile, country, countryParentNameGetterFn, builder);

  SerializeCrossMwm(mwmFile, CROSS_MWM_FILE_TAG, builder);
}

void BuildTransitCrossMwmSection(
    string const & path, string const & mwmFile, string const & country,
    CountryParentNameGetterFn const & countryParentNameGetterFn,
    ::transit::experimental::EdgeIdToFeatureId const & edgeIdToFeatureId, bool experimentalTransit)
{
  LOG(LINFO, ("Building transit cross mwm section for", country));
  CrossMwmConnectorBuilderEx<connector::TransitId> builder;

  CalcCrossMwmConnectors(path, mwmFile, "" /* intermediateDir */, country,
                         countryParentNameGetterFn, "" /* mapping file */, edgeIdToFeatureId,
                         builder, experimentalTransit);

  SerializeCrossMwm(mwmFile, TRANSIT_CROSS_MWM_FILE_TAG, builder);
}
}  // namespace routing_builder
