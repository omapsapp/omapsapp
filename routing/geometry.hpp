#pragma once

#include "routing/latlon_with_altitude.hpp"
#include "routing/road_point.hpp"
#include "routing/routing_options.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/mwm_set.hpp"

#include "geometry/latlon.hpp"

#include "base/buffer_vector.hpp"
#include "base/fifo_cache.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "3party/skarupke/bytell_hash_map.hpp"

class DataSource;

namespace routing
{
// @TODO(bykoianko) Consider setting cache size based on available memory.
// Maximum road geometry cache size in items.
size_t constexpr kRoadsCacheSize = 5000;

class RoadAttrsGetter;

class RoadGeometry final
{
public:
  using Points = buffer_vector<m2::PointD, 32>;

  RoadGeometry() = default;
  RoadGeometry(bool oneWay, double weightSpeedKMpH, double etaSpeedKMpH, Points const & points);

  /// @param[in] altitudes May be nullptr.
  void Load(VehicleModelInterface const & vehicleModel, FeatureType & feature,
            geometry::Altitudes const * altitudes, RoadAttrsGetter & attrs);

  bool IsOneWay() const { return m_isOneWay; }
  SpeedKMpH const & GetSpeed(bool forward) const;
  std::optional<HighwayType> GetHighwayType() const { return m_highwayType; }
  bool IsPassThroughAllowed() const { return m_isPassThroughAllowed; }

  LatLonWithAltitude const & GetJunction(uint32_t junctionId) const
  {
    ASSERT_LESS(junctionId, m_junctions.size(), ());
    return m_junctions[junctionId];
  }

  ms::LatLon const & GetPoint(uint32_t pointId) const { return GetJunction(pointId).GetLatLon(); }

  uint32_t GetPointsCount() const { return static_cast<uint32_t>(m_junctions.size()); }

  // Note. It's possible that car_model was changed after the map was built.
  // For example, the map from 12.2016 contained highway=pedestrian
  // in car_model but this type of highways is removed as of 01.2017.
  // In such cases RoadGeometry is not valid.
  bool IsValid() const { return m_valid; }

  bool IsEndPointId(uint32_t pointId) const
  {
    ASSERT_LESS(pointId, m_junctions.size(), ());
    return pointId == 0 || pointId + 1 == GetPointsCount();
  }

  void SetPassThroughAllowedForTests(bool passThroughAllowed)
  {
    m_isPassThroughAllowed = passThroughAllowed;
  }

  bool SuitableForOptions(RoutingOptions avoidRoutingOptions) const
  {
    return (avoidRoutingOptions.GetOptions() & m_routingOptions.GetOptions()) == 0;
  }

  RoutingOptions GetRoutingOptions() const { return m_routingOptions; }

  double GetRoadLengthM() const;

private:
  buffer_vector<LatLonWithAltitude, 32> m_junctions;

  SpeedKMpH m_forwardSpeed;
  SpeedKMpH m_backwardSpeed;
  std::optional<HighwayType> m_highwayType;
  bool m_isOneWay = false;
  bool m_valid = false;
  bool m_isPassThroughAllowed = false;
  RoutingOptions m_routingOptions;
};

class GeometryLoader
{
public:
  virtual ~GeometryLoader() = default;

  virtual void Load(uint32_t featureId, RoadGeometry & road) = 0;

  using VehicleModelPtrT = std::shared_ptr<VehicleModelInterface>;

  /// @param[in] handle should be alive, its caller responsibility to check it.
  static std::unique_ptr<GeometryLoader> Create(DataSource const & dataSource,
                                                MwmSet::MwmHandle const & handle,
                                                VehicleModelPtrT const & vehicleModel,
                                                bool loadAltitudes);

  /// This is for stand-alone work.
  /// Use in generator_tool and unit tests.
  static std::unique_ptr<GeometryLoader> CreateFromFile(
      std::string const & filePath, VehicleModelPtrT const & vehicleModel);
};

/// \brief This class supports loading geometry of roads for routing.
/// \note Loaded information about road geometry is kept in a fixed-size cache |m_featureIdToRoad|.
/// On the other hand methods GetRoad() and GetPoint() return geometry information by reference.
/// The reference may be invalid after the next call of GetRoad() or GetPoint() because the cache
/// item which is referred by returned reference may be evicted. It's done for performance reasons.
/// \note The cache |m_featureIdToRoad| is used for road geometry for single-directional
/// and bidirectional A*. According to tests it's faster to use one cache for both directions
/// in bidirectional A* case than two separate caches, one for each direction (one for each A* wave).
class Geometry final
{
public:
  Geometry() = default;
  /// \brief Geometry constructor
  /// \param roadsCacheSize in-memory geometry elements count limit
  Geometry(std::unique_ptr<GeometryLoader> loader, size_t roadsCacheSize = kRoadsCacheSize);

  /// \note The reference returned by the method is valid until the next call of GetRoad()
  /// of GetPoint() methods.
  RoadGeometry const & GetRoad(uint32_t featureId);

  /// \note The reference returned by the method is valid until the next call of GetRoad()
  /// of GetPoint() methods.
  ms::LatLon const & GetPoint(RoadPoint const & rp)
  {
    return GetRoad(rp.GetFeatureId()).GetPoint(rp.GetPointId());
  }

private:
  using RoutingFifoCache =
      FifoCache<uint32_t, RoadGeometry, ska::bytell_hash_map<uint32_t, RoadGeometry>>;

  std::unique_ptr<GeometryLoader> m_loader;
  std::unique_ptr<RoutingFifoCache> m_featureIdToRoad;
};
}  // namespace routing
