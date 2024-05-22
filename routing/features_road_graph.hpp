#pragma once

#include "routing/road_graph.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/altitude_loader.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include "base/cache.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

class FeatureType;

namespace routing
{
class MwmDataSource;

class FeaturesRoadGraphBase : public IRoadGraph
{
protected:
  using VehicleModelFactoryPtrT = std::shared_ptr<VehicleModelFactoryInterface>;

private:
  class CrossCountryVehicleModel
  {
    using FeatureTypes = VehicleModelInterface::FeatureTypes;

  public:
    explicit CrossCountryVehicleModel(VehicleModelFactoryPtrT modelFactory);

    // VehicleModelInterface overrides:
    SpeedKMpH GetSpeed(FeatureType & f, SpeedParams const & speedParams) const;
    std::optional<HighwayType> GetHighwayType(FeatureType & f) const;
    double GetMaxWeightSpeed() const { return m_maxSpeed; }
    SpeedKMpH const & GetOffroadSpeed() const;
    bool IsOneWay(FeatureType & f) const;
    bool IsRoad(FeatureType & f) const;
    bool IsPassThroughAllowed(FeatureType & f) const;

    void Clear();

  private:
    VehicleModelInterface * GetVehicleModel(FeatureID const & featureId) const;

    VehicleModelFactoryPtrT const m_modelFactory;
    double const m_maxSpeed;
    SpeedKMpH const m_offroadSpeedKMpH;

    mutable std::map<MwmSet::MwmId, std::shared_ptr<VehicleModelInterface>> m_cache;
  };

  class RoadInfoCache
  {
  public:
    RoadInfo & Find(FeatureID const & featureId, bool & found);

    void Clear();

  private:
    using TMwmFeatureCache = base::Cache<uint32_t, RoadInfo>;

    std::mutex m_mutexCache;
    std::map<MwmSet::MwmId, TMwmFeatureCache> m_cache;
  };

public:
  static double constexpr kClosestEdgesRadiusM = 150.0;

  FeaturesRoadGraphBase(MwmDataSource & dataSource, IRoadGraph::Mode mode, VehicleModelFactoryPtrT modelFactory);

  static int GetStreetReadScale();

  /// @name IRoadGraph overrides
  /// @{
  void ForEachFeatureClosestToCross(m2::PointD const & cross,
                                    ICrossEdgesLoader & edgesLoader) const override;
  void FindClosestEdges(m2::RectD const & rect, uint32_t count,
                        std::vector<std::pair<Edge, geometry::PointWithAltitude>> & vicinities) const override;
  std::vector<IRoadGraph::FullRoadInfo> FindRoads(
        m2::RectD const & rect, IsGoodFeatureFn const & isGoodFeature) const override;
  void GetFeatureTypes(FeatureID const & featureId, feature::TypesHolder & types) const override;
  void GetJunctionTypes(geometry::PointWithAltitude const & junction,
                        feature::TypesHolder & types) const override;
  IRoadGraph::Mode GetMode() const override;
  void ClearState() override;
  /// @}

  bool IsRoad(FeatureType & ft) const;

protected:
  MwmDataSource & m_dataSource;

  virtual feature::AltitudeLoaderBase * GetAltitudesLoader(MwmSet::MwmId const &) const
  {
    // Don't retrieve altitudes here because FeaturesRoadGraphBase is used in IndexRouter for
    // IndexRouter::FindClosestProjectionToRoad and IndexRouter::FindBestEdges only.
    return nullptr;
  }

private:
  friend class CrossFeaturesLoader;

  bool IsOneWay(FeatureType & ft) const;
  double GetSpeedKMpHFromFt(FeatureType & ft, SpeedParams const & speedParams) const;

  // Searches a feature RoadInfo in the cache, and if does not find then
  // loads feature from the index and takes speed for the feature from the vehicle model.
  RoadInfo const & GetCachedRoadInfo(FeatureID const & featureId, SpeedParams const & speedParams) const;
  // Searches a feature RoadInfo in the cache, and if does not find then takes passed feature and speed.
  // This version is used to prevent redundant feature loading when feature speed is known.
  RoadInfo const & GetCachedRoadInfo(FeatureID const & featureId, FeatureType & ft, double speedKMPH) const;
  void ExtractRoadInfo(FeatureID const & featureId, FeatureType & ft, double speedKMpH, RoadInfo & ri) const;

  IRoadGraph::Mode const m_mode;
  mutable RoadInfoCache m_cache;
  mutable CrossCountryVehicleModel m_vehicleModel;
};

class FeaturesRoadGraph : public FeaturesRoadGraphBase
{
  mutable std::map<MwmSet::MwmId, feature::AltitudeLoaderCached> m_altitudes;

public:
  FeaturesRoadGraph(MwmDataSource & dataSource, IRoadGraph::Mode mode, VehicleModelFactoryPtrT modelFactory)
    : FeaturesRoadGraphBase(dataSource, mode, modelFactory)
  {
  }

protected:
  feature::AltitudeLoaderCached * GetAltitudesLoader(MwmSet::MwmId const & mwmId) const override;
  void ClearState() override;
};

// @returns a distance d such as that for a given point p any edge
// with start point s such as that |s - p| < d, and edge is considered outgouing from p.
// Symmetrically for ingoing edges.
double GetRoadCrossingRadiusMeters();
}  // namespace routing
