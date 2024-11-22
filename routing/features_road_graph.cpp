#include "routing/features_road_graph.hpp"

#include "routing/data_source.hpp"
#include "routing/nearest_edge_finder.hpp"
#include "routing/routing_helpers.hpp"

#include "routing_common/vehicle_model.hpp"

#include "coding/point_coding.hpp"

#include <limits>

namespace routing
{
using namespace std;

namespace
{
uint32_t constexpr kPowOfTwoForFeatureCacheSize = 10; // cache contains 2 ^ kPowOfTwoForFeatureCacheSize elements

double constexpr kMwmRoadCrossingRadiusMeters = 2.0;

auto constexpr kInvalidSpeedKMPH = numeric_limits<double>::max();
}  // namespace

double GetRoadCrossingRadiusMeters() { return kMwmRoadCrossingRadiusMeters; }

FeaturesRoadGraphBase::CrossCountryVehicleModel::CrossCountryVehicleModel(VehicleModelFactoryPtrT modelFactory)
  : m_modelFactory(modelFactory)
  , m_maxSpeed(m_modelFactory->GetVehicleModel()->GetMaxWeightSpeed())
  , m_offroadSpeedKMpH(m_modelFactory->GetVehicleModel()->GetOffroadSpeed())
{
}

SpeedKMpH FeaturesRoadGraphBase::CrossCountryVehicleModel::GetSpeed(FeatureType & f, SpeedParams const & speedParams) const
{
  return GetVehicleModel(f.GetID())->GetSpeed(FeatureTypes(f), speedParams);
}

std::optional<HighwayType> FeaturesRoadGraphBase::CrossCountryVehicleModel::GetHighwayType(FeatureType & f) const
{
  return GetVehicleModel(f.GetID())->GetHighwayType(FeatureTypes(f));
}

SpeedKMpH const & FeaturesRoadGraphBase::CrossCountryVehicleModel::GetOffroadSpeed() const
{
  return m_offroadSpeedKMpH;
}

bool FeaturesRoadGraphBase::CrossCountryVehicleModel::IsOneWay(FeatureType & f) const
{
  return GetVehicleModel(f.GetID())->IsOneWay(FeatureTypes(f));
}

bool FeaturesRoadGraphBase::CrossCountryVehicleModel::IsRoad(FeatureType & f) const
{
  return GetVehicleModel(f.GetID())->IsRoad(FeatureTypes(f));
}

bool FeaturesRoadGraphBase::CrossCountryVehicleModel::IsPassThroughAllowed(FeatureType & f) const
{
  return GetVehicleModel(f.GetID())->IsPassThroughAllowed(FeatureTypes(f));
}

VehicleModelInterface * FeaturesRoadGraphBase::CrossCountryVehicleModel::GetVehicleModel(FeatureID const & featureId) const
{
  auto itr = m_cache.find(featureId.m_mwmId);
  if (itr != m_cache.end())
    return itr->second.get();

  auto vehicleModel = m_modelFactory->GetVehicleModelForCountry(featureId.m_mwmId.GetInfo()->GetCountryName());

  ASSERT(vehicleModel, ());
  ASSERT_EQUAL(m_maxSpeed, vehicleModel->GetMaxWeightSpeed(), ());

  itr = m_cache.emplace(featureId.m_mwmId, std::move(vehicleModel)).first;
  return itr->second.get();
}

void FeaturesRoadGraphBase::CrossCountryVehicleModel::Clear()
{
  m_cache.clear();
}

IRoadGraph::RoadInfo & FeaturesRoadGraphBase::RoadInfoCache::Find(FeatureID const & featureId, bool & found)
{
  std::lock_guard lock(m_mutexCache);
  auto res = m_cache.emplace(featureId.m_mwmId, TMwmFeatureCache());
  if (res.second)
    res.first->second.Init(kPowOfTwoForFeatureCacheSize);
  return res.first->second.Find(featureId.m_index, found);
}

void FeaturesRoadGraph::RoadInfoCache::Clear()
{
  std::lock_guard lock(m_mutexCache);
  m_cache.clear();
}

FeaturesRoadGraphBase::FeaturesRoadGraphBase(MwmDataSource & dataSource, IRoadGraph::Mode mode,
                                             shared_ptr<VehicleModelFactoryInterface> vehicleModelFactory)
  : m_dataSource(dataSource), m_mode(mode), m_vehicleModel(vehicleModelFactory)
{
}

class CrossFeaturesLoader
{
public:
  CrossFeaturesLoader(FeaturesRoadGraphBase const & graph, IRoadGraph::ICrossEdgesLoader & edgesLoader)
    : m_graph(graph), m_edgesLoader(edgesLoader)
  {}

  void operator()(FeatureType & ft)
  {
    if (!m_graph.IsRoad(ft))
      return;

    FeatureID const & featureId = ft.GetID();

    IRoadGraph::RoadInfo const & roadInfo = m_graph.GetCachedRoadInfo(featureId, ft, kInvalidSpeedKMPH);
    CHECK_EQUAL(roadInfo.m_speedKMPH, kInvalidSpeedKMPH, ());

    m_edgesLoader(featureId, roadInfo.m_junctions, roadInfo.m_bidirectional);
  }

private:
  FeaturesRoadGraphBase const & m_graph;
  IRoadGraph::ICrossEdgesLoader & m_edgesLoader;
};

void FeaturesRoadGraphBase::ForEachFeatureClosestToCross(
      m2::PointD const & cross, ICrossEdgesLoader & edgesLoader) const
{
  CrossFeaturesLoader featuresLoader(*this, edgesLoader);
  m2::RectD const rect = mercator::RectByCenterXYAndSizeInMeters(cross, kMwmRoadCrossingRadiusMeters);
  m_dataSource.ForEachStreet(featuresLoader, rect);
}

void FeaturesRoadGraphBase::FindClosestEdges(m2::RectD const & rect, uint32_t count,
                                             vector<pair<Edge, geometry::PointWithAltitude>> & vicinities) const
{
  NearestEdgeFinder finder(rect.Center(), nullptr /* IsEdgeProjGood */);

  m_dataSource.ForEachStreet([&](FeatureType & ft)
  {
    if (!m_vehicleModel.IsRoad(ft))
      return;

    FeatureID const & featureId = ft.GetID();

    IRoadGraph::RoadInfo const & roadInfo = GetCachedRoadInfo(featureId, ft, kInvalidSpeedKMPH);
    finder.AddInformationSource(IRoadGraph::FullRoadInfo(featureId, roadInfo));
  }, rect);

  finder.MakeResult(vicinities, count);
}

vector<IRoadGraph::FullRoadInfo>
FeaturesRoadGraphBase::FindRoads(m2::RectD const & rect, IsGoodFeatureFn const & isGoodFeature) const
{
  vector<IRoadGraph::FullRoadInfo> roads;

  m_dataSource.ForEachStreet([&](FeatureType & ft)
  {
    if (!m_vehicleModel.IsRoad(ft))
      return;

    FeatureID const & featureId = ft.GetID();
    if (isGoodFeature && !isGoodFeature(featureId))
      return;

    // DataSource::ForEachInRect() gives not only features inside |rect| but some other features
    // which lie close to the rect. Removes all the features which don't cross |rect|.
    auto const & roadInfo = GetCachedRoadInfo(featureId, ft, kInvalidSpeedKMPH);
    if (!RectCoversPolyline(roadInfo.m_junctions, rect))
      return;

    roads.emplace_back(featureId, roadInfo);
  }, rect);

  return roads;
}

void FeaturesRoadGraphBase::GetFeatureTypes(FeatureID const & featureId, feature::TypesHolder & types) const
{
  auto ft = m_dataSource.GetFeature(featureId);
  if (!ft)
    return;

  ASSERT_EQUAL(ft->GetGeomType(), feature::GeomType::Line, ());
  types = feature::TypesHolder(*ft);
}

void FeaturesRoadGraphBase::GetJunctionTypes(geometry::PointWithAltitude const & junction,
                                             feature::TypesHolder & types) const
{
  types = feature::TypesHolder();

  m2::PointD const & cross = junction.GetPoint();
  m2::RectD const rect = mercator::RectByCenterXYAndSizeInMeters(cross, kMwmRoadCrossingRadiusMeters);

  m_dataSource.ForEachStreet([&](FeatureType & ft)
  {
    if (types.Empty() && ft.GetGeomType() == feature::GeomType::Point &&
        AlmostEqualAbs(ft.GetCenter(), cross, kMwmPointAccuracy))
    {
      types = feature::TypesHolder(ft);
    }
  }, rect);
}

IRoadGraph::Mode FeaturesRoadGraphBase::GetMode() const
{
  return m_mode;
}

void FeaturesRoadGraphBase::ClearState()
{
  m_cache.Clear();
  m_vehicleModel.Clear();
}

bool FeaturesRoadGraphBase::IsRoad(FeatureType & ft) const
{
  return m_vehicleModel.IsRoad(ft);
}

bool FeaturesRoadGraphBase::IsOneWay(FeatureType & ft) const
{
  return m_vehicleModel.IsOneWay(ft);
}

double FeaturesRoadGraphBase::GetSpeedKMpHFromFt(FeatureType & ft, SpeedParams const & speedParams) const
{
  return m_vehicleModel.GetSpeed(ft, speedParams).m_weight;
}

void FeaturesRoadGraphBase::ExtractRoadInfo(FeatureID const & featureId, FeatureType & ft,
                                            double speedKMpH, RoadInfo & ri) const
{
  ri.m_speedKMPH = speedKMpH;
  ri.m_bidirectional = !IsOneWay(ft);

  ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
  size_t const pointsCount = ft.GetPointsCount();

  geometry::Altitudes altitudes;
  auto const loader = GetAltitudesLoader(featureId.m_mwmId);
  if (loader)
    altitudes = loader->GetAltitudes(featureId.m_index, pointsCount);
  else
    altitudes = geometry::Altitudes(pointsCount, geometry::kDefaultAltitudeMeters);

  CHECK_EQUAL(altitudes.size(), pointsCount, ("GetAltitudes for", featureId, "returns wrong altitudes:", altitudes));

  ri.m_junctions.resize(pointsCount);
  for (size_t i = 0; i < pointsCount; ++i)
  {
    auto & pt = ri.m_junctions[i];
    pt.SetPoint(ft.GetPoint(i));
    pt.SetAltitude(altitudes[i]);
  }
}

IRoadGraph::RoadInfo const & FeaturesRoadGraphBase::GetCachedRoadInfo(
      FeatureID const & featureId, SpeedParams const & speedParams) const
{
  bool found = false;
  RoadInfo & ri = m_cache.Find(featureId, found);

  if (found)
    return ri;

  auto ft = m_dataSource.GetFeature(featureId);
  if (!ft)
    return ri;

  ASSERT_EQUAL(ft->GetGeomType(), feature::GeomType::Line, ());

  ExtractRoadInfo(featureId, *ft, GetSpeedKMpHFromFt(*ft, speedParams), ri);
  return ri;
}

IRoadGraph::RoadInfo const & FeaturesRoadGraphBase::GetCachedRoadInfo(
      FeatureID const & featureId, FeatureType & ft, double speedKMPH) const
{
  bool found = false;
  RoadInfo & ri = m_cache.Find(featureId, found);
  if (found)
    return ri;

  ASSERT_EQUAL(featureId, ft.GetID(), ());
  ExtractRoadInfo(featureId, ft, speedKMPH, ri);
  return ri;
}

feature::AltitudeLoaderCached * FeaturesRoadGraph::GetAltitudesLoader(MwmSet::MwmId const & mwmId) const
{
  auto it = m_altitudes.find(mwmId);
  if (it == m_altitudes.end())
  {
    auto const & handle = m_dataSource.GetHandle(mwmId);
    it = m_altitudes.emplace(mwmId, *handle.GetValue()).first;
  }
  return &(it->second);
}

void FeaturesRoadGraph::ClearState()
{
  FeaturesRoadGraphBase::ClearState();
  m_altitudes.clear();
}

}  // namespace routing
