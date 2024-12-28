#include "routing/index_graph_loader.hpp"

#include "routing/data_source.hpp"
#include "routing/index_graph_serialization.hpp"
#include "routing/restriction_loader.hpp"
#include "routing/road_access.hpp"
#include "routing/road_access_serialization.hpp"
#include "routing/route.hpp"
#include "routing/speed_camera_ser_des.hpp"

#include "coding/files_container.hpp"

#include "base/assert.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>


namespace routing
{
namespace
{
using namespace std;

class IndexGraphLoaderImpl final : public IndexGraphLoader
{
public:
  IndexGraphLoaderImpl(VehicleType vehicleType, bool loadAltitudes,
                       shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
                       shared_ptr<EdgeEstimator> estimator, MwmDataSource & dataSource,
                       RoutingOptions routingOptions = RoutingOptions())
    : m_vehicleType(vehicleType)
    , m_loadAltitudes(loadAltitudes)
    , m_dataSource(dataSource)
    , m_vehicleModelFactory(std::move(vehicleModelFactory))
    , m_estimator(std::move(estimator))
    , m_avoidRoutingOptions(routingOptions)
  {
    CHECK(m_vehicleModelFactory, ());
    CHECK(m_estimator, ());
  }

  // IndexGraphLoader overrides:
  IndexGraph & GetIndexGraph(NumMwmId numMwmId) override;
  Geometry & GetGeometry(NumMwmId numMwmId) override;
  vector<RouteSegment::SpeedCamera> GetSpeedCameraInfo(Segment const & segment) override;
  void Clear() override;

private:
  using GeometryPtrT = shared_ptr<Geometry>;
  GeometryPtrT CreateGeometry(NumMwmId numMwmId);
  using GraphPtrT = unique_ptr<IndexGraph>;
  GraphPtrT CreateIndexGraph(NumMwmId numMwmId, GeometryPtrT & geometry);

  VehicleType m_vehicleType;
  bool m_loadAltitudes;
  MwmDataSource & m_dataSource;
  shared_ptr<VehicleModelFactoryInterface> m_vehicleModelFactory;
  shared_ptr<EdgeEstimator> m_estimator;

  struct GraphAttrs
  {
    GeometryPtrT m_geometry;
    // May be nullptr, because it has "lazy" loading.
    GraphPtrT m_graph;
  };
  unordered_map<NumMwmId, GraphAttrs> m_graphs;

  unordered_map<NumMwmId, SpeedCamerasMapT> m_cachedCameras;
  SpeedCamerasMapT const & ReceiveSpeedCamsFromMwm(NumMwmId numMwmId);

  RoutingOptions m_avoidRoutingOptions;
  std::function<time_t()> m_currentTimeGetter = [time = GetCurrentTimestamp()]() {
    return time;
  };
};

IndexGraph & IndexGraphLoaderImpl::GetIndexGraph(NumMwmId numMwmId)
{
  auto res = m_graphs.try_emplace(numMwmId, GraphAttrs());
  if (res.second || res.first->second.m_graph == nullptr)
  {
    // Create graph using (or initializing) existing geometry.
    res.first->second.m_graph = CreateIndexGraph(numMwmId, res.first->second.m_geometry);
  }
  return *(res.first->second.m_graph);
}

Geometry & IndexGraphLoaderImpl::GetGeometry(NumMwmId numMwmId)
{
  auto res = m_graphs.try_emplace(numMwmId, GraphAttrs());
  if (res.second)
  {
    // Create geometry only, graph stays nullptr.
    res.first->second.m_geometry = CreateGeometry(numMwmId);
  }
  return *(res.first->second.m_geometry);
}

SpeedCamerasMapT const & IndexGraphLoaderImpl::ReceiveSpeedCamsFromMwm(NumMwmId numMwmId)
{
  auto res = m_cachedCameras.try_emplace(numMwmId, SpeedCamerasMapT{});
  if (res.second)
    (void)ReadSpeedCamsFromMwm(m_dataSource.GetMwmValue(numMwmId), res.first->second);
  return res.first->second;
}

vector<RouteSegment::SpeedCamera> IndexGraphLoaderImpl::GetSpeedCameraInfo(Segment const & segment)
{
  SpeedCamerasMapT const & camerasMap = ReceiveSpeedCamsFromMwm(segment.GetMwmId());
  auto it = camerasMap.find({segment.GetFeatureId(), segment.GetSegmentIdx()});
  if (it == camerasMap.end())
    return {};

  auto cameras = it->second;
  base::SortUnique(cameras, std::less<RouteSegment::SpeedCamera>(), [](auto const & l, auto const & r)
  {
    return l.EqualCoef(r);
  });

  // Cameras stored from beginning to ending of segment. So if we go at segment in backward direction,
  // we should read cameras in reverse sequence too.
  if (!segment.IsForward())
    std::reverse(cameras.begin(), cameras.end());

  return cameras;
}

IndexGraphLoaderImpl::GraphPtrT IndexGraphLoaderImpl::CreateIndexGraph(NumMwmId numMwmId, GeometryPtrT & geometry)
{
  MwmSet::MwmHandle const & handle = m_dataSource.GetHandle(numMwmId);
  MwmValue const * value = handle.GetValue();

  try
  {
    base::Timer timer;

    if (!geometry)
    {
      auto vehicleModel = m_vehicleModelFactory->GetVehicleModelForCountry(value->GetCountryFileName());
      geometry = make_shared<Geometry>(GeometryLoader::Create(handle, std::move(vehicleModel), m_loadAltitudes));
    }

    auto graph = make_unique<IndexGraph>(geometry, m_estimator, m_avoidRoutingOptions);
    graph->SetCurrentTimeGetter(m_currentTimeGetter);
    DeserializeIndexGraph(*value, m_vehicleType, *graph);

    LOG(LINFO, (ROUTING_FILE_TAG, "section for", value->GetCountryFileName(), "loaded in", timer.ElapsedSeconds(), "seconds"));
    return graph;
  }
  catch (RootException const & ex)
  {
    LOG(LERROR, ("Error reading graph for", value->m_file));
    throw;
  }
}

IndexGraphLoaderImpl::GeometryPtrT IndexGraphLoaderImpl::CreateGeometry(NumMwmId numMwmId)
{
  MwmSet::MwmHandle const & handle = m_dataSource.GetHandle(numMwmId);
  MwmValue const * value = handle.GetValue();

  auto vehicleModel = m_vehicleModelFactory->GetVehicleModelForCountry(value->GetCountryFileName());
  return make_shared<Geometry>(GeometryLoader::Create(handle, std::move(vehicleModel), m_loadAltitudes));
}

void IndexGraphLoaderImpl::Clear() { m_graphs.clear(); }

} // namespace

bool ReadSpeedCamsFromMwm(MwmValue const & mwmValue, SpeedCamerasMapT & camerasMap)
{
  try
  {
    auto reader = mwmValue.m_cont.GetReader(CAMERAS_INFO_FILE_TAG);
    ReaderSource src(reader);
    DeserializeSpeedCamsFromMwm(src, camerasMap);
    return true;
  }
  catch (Reader::OpenException const &)
  {
    LOG(LWARNING, (CAMERAS_INFO_FILE_TAG "section not found"));
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Error while reading", CAMERAS_INFO_FILE_TAG, "section.", e.Msg()));
  }
  return false;
}

bool ReadRoadAccessFromMwm(MwmValue const & mwmValue, VehicleType vehicleType, RoadAccess & roadAccess)
{
  try
  {
    auto const reader = mwmValue.m_cont.GetReader(ROAD_ACCESS_FILE_TAG);
    ReaderSource src(reader);
    RoadAccessSerializer::Deserialize(src, vehicleType, roadAccess);
    return true;
  }
  catch (Reader::OpenException const &)
  {
    LOG(LWARNING, (ROAD_ACCESS_FILE_TAG, "section not found"));
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Error while reading", ROAD_ACCESS_FILE_TAG, "section.", e.Msg()));
  }
  return false;
}

// static
unique_ptr<IndexGraphLoader> IndexGraphLoader::Create(
    VehicleType vehicleType, bool loadAltitudes,
    shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory,
    shared_ptr<EdgeEstimator> estimator, MwmDataSource & dataSource,
    RoutingOptions routingOptions)
{
  return make_unique<IndexGraphLoaderImpl>(vehicleType, loadAltitudes, vehicleModelFactory,
                                           estimator, dataSource, routingOptions);
}

void DeserializeIndexGraph(MwmValue const & mwmValue, VehicleType vehicleType, IndexGraph & graph)
{
  FilesContainerR::TReader reader(mwmValue.m_cont.GetReader(ROUTING_FILE_TAG));
  ReaderSource<FilesContainerR::TReader> src(reader);

  IndexGraphSerializer::Deserialize(graph, src, GetVehicleMask(vehicleType));

  // Do not load restrictions (relation type = restriction) for pedestrian routing.
  // https://wiki.openstreetmap.org/wiki/Relation:restriction
  /// @todo OSM has 49 (April 2022) restriction:foot relations. We should use them someday,
  /// starting from generator and saving like access, according to the vehicleType.
  ASSERT(vehicleType != VehicleType::Transit, ());
  if (vehicleType != VehicleType::Pedestrian)
  {
    RestrictionLoader restrictionLoader(mwmValue, graph);
    if (restrictionLoader.HasRestrictions())
    {
      graph.SetRestrictions(restrictionLoader.StealRestrictions());
      graph.SetUTurnRestrictions(restrictionLoader.StealNoUTurnRestrictions());
    }
  }

  RoadAccess roadAccess;
  if (ReadRoadAccessFromMwm(mwmValue, vehicleType, roadAccess))
    graph.SetRoadAccess(std::move(roadAccess));
}

uint32_t DeserializeIndexGraphNumRoads(MwmValue const & mwmValue, VehicleType vehicleType)
{
  FilesContainerR::TReader reader(mwmValue.m_cont.GetReader(ROUTING_FILE_TAG));
  ReaderSource<FilesContainerR::TReader> src(reader);
  return IndexGraphSerializer::DeserializeNumRoads(src, GetVehicleMask(vehicleType));
}
}  // namespace routing
