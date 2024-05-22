#include "routing/geometry.hpp"

#include "routing/city_roads.hpp"
#include "routing/maxspeeds.hpp"

#include "indexer/altitude_loader.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_source.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include <string>

namespace routing
{
using namespace std;

double CalcFerryDurationHours(string_view durationHours, double roadLenKm)
{
  // Look for more info: https://confluence.mail.ru/display/MAPSME/Ferries
  // Shortly: the coefs were received from statistic about ferries with durations in OSM.
  double constexpr kIntercept = 0.2490726747447476;
  /// @todo This constant means that average ferry speed is 1/0.02 = 50km/h OMG!
  double constexpr kSlope = 0.02078913;

  if (durationHours.empty())
    return kIntercept + kSlope * roadLenKm;

  double durationH = 0.0;
  CHECK(strings::to_double(durationHours, durationH), (durationHours));

  // See: https://confluence.mail.ru/download/attachments/249123157/image2019-8-22_16-15-53.png
  // Shortly: we drop some points: (x: lengthKm, y: durationH), that are upper or lower these two lines.
  double constexpr kUpperBoundIntercept = 4.0;
  double constexpr kUpperBoundSlope = 0.037;
  if (kUpperBoundIntercept + kUpperBoundSlope * roadLenKm - durationH < 0)
    return kIntercept + kSlope * roadLenKm;

  double constexpr kLowerBoundIntercept = -2.0;
  double constexpr kLowerBoundSlope = 0.015;
  if (kLowerBoundIntercept + kLowerBoundSlope * roadLenKm - durationH > 0)
    return kIntercept + kSlope * roadLenKm;

  return durationH;
}

class RoadAttrsGetter
{
public:
  void Load(FilesContainerR const & cont)
  {
    try
    {
      if (cont.IsExist(CITY_ROADS_FILE_TAG))
        m_cityRoads.Load(cont.GetReader(CITY_ROADS_FILE_TAG));

      if (cont.IsExist(MAXSPEEDS_FILE_TAG))
        m_maxSpeeds.Load(cont.GetReader(MAXSPEEDS_FILE_TAG));
    }
    catch (Reader::Exception const & e)
    {
      LOG(LERROR, ("File", cont.GetFileName(), "Error while reading", CITY_ROADS_FILE_TAG, "or",
                   MAXSPEEDS_FILE_TAG, "section.", e.Msg()));
    }
  }

public:
  Maxspeeds m_maxSpeeds;
  CityRoads m_cityRoads;
};

namespace
{
class GeometryLoaderImpl final : public GeometryLoader
{
public:
  GeometryLoaderImpl(MwmSet::MwmHandle const & handle, VehicleModelPtrT const & vehicleModel, bool loadAltitudes)
    : m_vehicleModel(vehicleModel)
    , m_source(handle)
    , m_altitudeLoader(*handle.GetValue())
    , m_loadAltitudes(loadAltitudes)
  {
    m_attrsGetter.Load(handle.GetValue()->m_cont);
  }

  void Load(uint32_t featureId, RoadGeometry & road) override
  {
    auto feature = m_source.GetOriginalFeature(featureId);
    feature->ParseGeometry(FeatureType::BEST_GEOMETRY);

    geometry::Altitudes altitudes;
    if (m_loadAltitudes)
      altitudes = m_altitudeLoader.GetAltitudes(featureId, feature->GetPointsCount());

    road.Load(*m_vehicleModel, *feature, altitudes.empty() ? nullptr : &altitudes, m_attrsGetter);
  }

  SpeedInUnits GetSavedMaxspeed(uint32_t featureId, bool forward) override
  {
    auto const speed = m_attrsGetter.m_maxSpeeds.GetMaxspeed(featureId);
    return { speed.GetSpeedInUnits(forward), speed.GetUnits() };
  }

private:
  VehicleModelPtrT m_vehicleModel;
  RoadAttrsGetter m_attrsGetter;
  FeatureSource m_source;
  feature::AltitudeLoaderBase m_altitudeLoader;
  bool const m_loadAltitudes;
};

class FileGeometryLoader final : public GeometryLoader
{
public:
  FileGeometryLoader(string const & fileName, VehicleModelPtrT const & vehicleModel)
    : m_featuresVector(fileName)
    , m_vehicleModel(vehicleModel)
  {
    m_attrsGetter.Load(m_featuresVector.GetContainer());
  }

  void Load(uint32_t featureId, RoadGeometry & road) override
  {
    auto feature = m_featuresVector.GetVector().GetByIndex(featureId);
    CHECK(feature, ());
    feature->SetID({{}, featureId});
    feature->ParseGeometry(FeatureType::BEST_GEOMETRY);

    // Note. If FileGeometryLoader is used for generation cross mwm section for bicycle or
    // pedestrian routing |altitudes| should be used here.
    road.Load(*m_vehicleModel, *feature, nullptr /* altitudes */, m_attrsGetter);
  }

private:
  FeaturesVectorTest m_featuresVector;
  RoadAttrsGetter m_attrsGetter;
  VehicleModelPtrT m_vehicleModel;
};
} // namespace


// RoadGeometry ------------------------------------------------------------------------------------
RoadGeometry::RoadGeometry(bool oneWay, double weightSpeedKMpH, double etaSpeedKMpH, Points const & points)
  : m_forwardSpeed{weightSpeedKMpH, etaSpeedKMpH}, m_backwardSpeed(m_forwardSpeed)
  , m_isOneWay(oneWay), m_valid(true), m_isPassThroughAllowed(false), m_inCity(false)
{
  ASSERT_GREATER(weightSpeedKMpH, 0.0, ());
  ASSERT_GREATER(etaSpeedKMpH, 0.0, ());

  size_t const count = points.size();
  ASSERT_GREATER(count, 1, ());

  m_junctions.reserve(count);
  for (auto const & point : points)
    m_junctions.emplace_back(mercator::ToLatLon(point), geometry::kDefaultAltitudeMeters);

  m_distances.resize(count - 1, -1);
}

void RoadGeometry::Load(VehicleModelInterface const & vehicleModel, FeatureType & feature,
                        geometry::Altitudes const * altitudes, RoadAttrsGetter & attrs)
{
  size_t const count = feature.GetPointsCount();
  CHECK(altitudes == nullptr || altitudes->size() == count, ());

  feature::TypesHolder types(feature);
  m_highwayType = vehicleModel.GetHighwayType(types);

  m_valid = vehicleModel.IsRoad(types);
  m_isOneWay = vehicleModel.IsOneWay(types);
  m_isPassThroughAllowed = vehicleModel.IsPassThroughAllowed(types);

  uint32_t const fID = feature.GetID().m_index;
  m_inCity = attrs.m_cityRoads.IsCityRoad(fID);

  SpeedParams params(attrs.m_maxSpeeds.GetMaxspeed(fID),
                     m_highwayType ? attrs.m_maxSpeeds.GetDefaultSpeed(m_inCity, *m_highwayType) : kInvalidSpeed,
                     m_inCity);
  params.m_forward = true;
  m_forwardSpeed = vehicleModel.GetSpeed(types, params);
  params.m_forward = false;
  m_backwardSpeed = vehicleModel.GetSpeed(types, params);

  auto const & optionsClassfier = RoutingOptionsClassifier::Instance();
  for (uint32_t type : types)
  {
    if (auto const it = optionsClassfier.Get(type))
      m_routingOptions.Add(*it);
  }

  m_junctions.clear();
  m_junctions.reserve(count);
  for (size_t i = 0; i < count; ++i)
  {
    auto const ll = mercator::ToLatLon(feature.GetPoint(i));
    m_junctions.emplace_back(ll, altitudes ? (*altitudes)[i] : geometry::kDefaultAltitudeMeters);

#ifdef DEBUG
    // I'd like to check these big jumps manually, if any.
    if (altitudes && i > 0)
    {
      // Since we store integer altitudes, 1 is a possible error for 2 points.
      geometry::Altitude constexpr kError = 1;

      auto const altDiff = abs((*altitudes)[i] - (*altitudes)[i-1]);
      if (altDiff > kError)
      {
        double const dist = ms::DistanceOnEarth(m_junctions[i-1].GetLatLon(), m_junctions[i].GetLatLon());
        if ((altDiff - kError) / dist > 0.3)
          LOG(LWARNING, ("Altitudes jump:", m_junctions[i-1], m_junctions[i]));
      }
    }
#endif
  }
  m_distances.resize(count - 1, -1);

  bool const isFerry = m_routingOptions.Has(RoutingOptions::Road::Ferry);
  /// @todo Add RouteShuttleTrain into RoutingOptions?
  if (isFerry || (m_highwayType && *m_highwayType == HighwayType::RouteShuttleTrain))
  {
    // Skip shuttle train calculation without duration.
    auto const durationMeta = feature.GetMetadata(feature::Metadata::FMD_DURATION);
    if (isFerry || !durationMeta.empty())
    {
      /// @todo Also process "interval" OSM tag (without additional boarding penalties).
      // https://github.com/organicmaps/organicmaps/issues/3695

      auto const roadLenKm = GetRoadLengthM() / 1000.0;
      double const durationH = CalcFerryDurationHours(durationMeta, roadLenKm);
      CHECK(!base::AlmostEqualAbs(durationH, 0.0, 1e-5), (durationH));

      if (roadLenKm != 0.0)
      {
        double const speed = roadLenKm / durationH;
        ASSERT_LESS_OR_EQUAL(speed, vehicleModel.GetMaxWeightSpeed(), (roadLenKm, durationH, fID));
        m_forwardSpeed = m_backwardSpeed = SpeedKMpH(speed);
      }
    }
  }

  if (m_valid)
  {
    ASSERT(m_forwardSpeed.IsValid() && m_backwardSpeed.IsValid(), (feature.DebugString()));
  }
}

double RoadGeometry::GetDistance(uint32_t idx) const
{
  if (m_distances[idx] < 0)
    m_distances[idx] = ms::DistanceOnEarth(m_junctions[idx].GetLatLon(), m_junctions[idx + 1].GetLatLon());
  return m_distances[idx];
}

SpeedKMpH const & RoadGeometry::GetSpeed(bool forward) const
{
  return forward ? m_forwardSpeed : m_backwardSpeed;
}

double RoadGeometry::GetRoadLengthM() const
{
  uint32_t const count = GetPointsCount();
  double lenM = 0.0;

  if (count > 0)
  {
    for (uint32_t i = 0; i < count - 1; ++i)
      lenM += GetDistance(i);
  }

  return lenM;
}

// Geometry ----------------------------------------------------------------------------------------
Geometry::Geometry(unique_ptr<GeometryLoader> loader, size_t roadsCacheSize)
  : m_loader(std::move(loader))
{
  CHECK(m_loader, ());

  m_featureIdToRoad = make_unique<RoutingCacheT>(roadsCacheSize, [this](uint32_t featureId, RoadGeometry & road)
  {
    m_loader->Load(featureId, road);
  });
}

RoadGeometry const & Geometry::GetRoad(uint32_t featureId)
{
  ASSERT(m_featureIdToRoad, ());
  ASSERT(m_loader, ());

  return m_featureIdToRoad->GetValue(featureId);
}

SpeedInUnits GeometryLoader::GetSavedMaxspeed(uint32_t, bool)
{
  UNREACHABLE();
}

// static
unique_ptr<GeometryLoader> GeometryLoader::Create(MwmSet::MwmHandle const & handle,
                                                  VehicleModelPtrT const & vehicleModel,
                                                  bool loadAltitudes)
{
  CHECK(handle.IsAlive(), ());
  CHECK(vehicleModel, ());
  return make_unique<GeometryLoaderImpl>(handle, vehicleModel, loadAltitudes);
}

// static
unique_ptr<GeometryLoader> GeometryLoader::CreateFromFile(
    string const & fileName, VehicleModelPtrT const & vehicleModel)
{
  CHECK(vehicleModel, ());
  return make_unique<FileGeometryLoader>(fileName, vehicleModel);
}
}  // namespace routing
