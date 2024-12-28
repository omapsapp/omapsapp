#pragma once

#include "routing_common/maxspeed_conversion.hpp"

#include "indexer/feature_data.hpp"

#include "base/small_map.hpp"

#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Classificator;
class FeatureType;

namespace routing
{
double constexpr kNotUsed = std::numeric_limits<double>::max();

struct InOutCityFactor;
struct InOutCitySpeedKMpH;

// Each value is equal to the corresponding 0-based type index from types.txt
// (an ID from mapcss-mapping.csv minus 1).
// The ascending order is strict. Check for static_cast<HighwayType> in vehicle_model.cpp
enum class HighwayType : uint16_t
{
  HighwayResidential = 1,
  HighwayService = 2,
  HighwayUnclassified = 4,
  HighwayFootway = 6,
  HighwayTrack = 7,
  HighwayTertiary = 8,
  HighwaySecondary = 12,
  HighwayPath = 15,
  HighwayPrimary = 26,
  HighwayRoad = 410,
  HighwayCycleway = 36,
  HighwayMotorwayLink = 43,
  HighwayLivingStreet = 54,
  HighwayMotorway = 57,
  HighwaySteps = 58,
  HighwayTrunk = 65,
  HighwayPedestrian = 69,
  HighwayTrunkLink = 90,
  HighwayPrimaryLink = 95,
  ManMadePier = 119,
  HighwayBridleway = 167,
  HighwaySecondaryLink = 176,
  RouteFerry = 259,
  HighwayTertiaryLink = 272,
  HighwayBusway = 857,    // reserve type here, but this type is not used for any routing by default
  RouteShuttleTrain = 1054,
};

using HighwayBasedFactors = base::SmallMap<HighwayType, InOutCityFactor>;
using HighwayBasedSpeeds = base::SmallMap<HighwayType, InOutCitySpeedKMpH>;

/// \brief Params for calculation of an approximate speed on a feature.
struct SpeedParams
{
  /// @deprecated For unit tests compatibility.
  SpeedParams(bool forward, bool inCity, Maxspeed const & maxspeed)
    : m_maxspeed(maxspeed), m_defSpeedKmPH(kInvalidSpeed), m_inCity(inCity), m_forward(forward)
  {
  }

  SpeedParams(Maxspeed const & maxspeed, MaxspeedType defSpeedKmPH, bool inCity)
    : m_maxspeed(maxspeed), m_defSpeedKmPH(defSpeedKmPH), m_inCity(inCity)
  {
  }

  // Maxspeed stored for feature, if any.
  Maxspeed m_maxspeed;
  // Default speed for this feature type in MWM, if any (kInvalidSpeed otherwise).
  MaxspeedType m_defSpeedKmPH;
  // If a corresponding feature lies inside a city of a town.
  bool m_inCity;
  // Retrieve forward (true) or backward (false) speed.
  bool m_forward;
};

/// \brief Speeds which are used for edge weight and ETA estimations.
struct SpeedKMpH
{
  constexpr SpeedKMpH() = default;
  constexpr SpeedKMpH(double weight) noexcept : m_weight(weight), m_eta(weight) {}
  constexpr SpeedKMpH(double weight, double eta) noexcept : m_weight(weight), m_eta(eta) {}

  bool operator==(SpeedKMpH const & rhs) const
  {
    return m_weight == rhs.m_weight && m_eta == rhs.m_eta;
  }
  bool operator!=(SpeedKMpH const & rhs) const { return !(*this == rhs); }

  bool operator<(SpeedKMpH const & rhs) const
  {
    return m_weight < rhs.m_weight && m_eta < rhs.m_eta;
  }

  bool IsValid() const { return m_weight > 0 && m_eta > 0; }

  double m_weight = 0.0;  // KMpH - speed in km/h adjusted for desirability
                          // cycling on very large road may be fast but speed used for route finding will be treated as much lower
  double m_eta = 0.0;     // KMpH - actual expected speed in km/h, used to display expected arrival time
};

/// \brief Factors which modify weight and ETA speed on feature in case of bad pavement (reduce)
/// or good highway class (increase).
/// Both should be greater then 0.
struct SpeedFactor
{
  constexpr SpeedFactor() = default;
  constexpr SpeedFactor(double factor) noexcept : m_weight(factor), m_eta(factor) {}
  constexpr SpeedFactor(double weight, double eta) noexcept : m_weight(weight), m_eta(eta) {}

  bool IsValid() const { return m_weight > 0.0 && m_weight <= 1.0 && m_eta > 0.0 && m_eta <= 1.0; }
  void SetMin(SpeedFactor const & f)
  {
    ASSERT(f.IsValid(), ());
    m_weight = std::max(f.m_weight, m_weight);
    m_eta = std::max(f.m_eta, m_eta);
  }

  bool operator==(SpeedFactor const & rhs) const
  {
    return m_weight == rhs.m_weight && m_eta == rhs.m_eta;
  }
  bool operator!=(SpeedFactor const & rhs) const { return !(*this == rhs); }

  double m_weight = 1.0;
  double m_eta = 1.0;
};

inline SpeedKMpH operator*(SpeedFactor const & factor, SpeedKMpH const & speed)
{
  return {speed.m_weight * factor.m_weight, speed.m_eta * factor.m_eta};
}

inline SpeedKMpH operator*(SpeedKMpH const & speed, SpeedFactor const & factor)
{
  return {speed.m_weight * factor.m_weight, speed.m_eta * factor.m_eta};
}

struct InOutCitySpeedKMpH
{
  constexpr InOutCitySpeedKMpH() = default;
  constexpr explicit InOutCitySpeedKMpH(SpeedKMpH const & speed) noexcept
    : m_inCity(speed), m_outCity(speed)
  {
  }
  constexpr InOutCitySpeedKMpH(SpeedKMpH const & inCity, SpeedKMpH const & outCity) noexcept
    : m_inCity(inCity), m_outCity(outCity)
  {
  }

  bool operator==(InOutCitySpeedKMpH const & rhs) const
  {
    return m_inCity == rhs.m_inCity && m_outCity == rhs.m_outCity;
  }

  SpeedKMpH const & GetSpeed(bool isCity) const { return isCity ? m_inCity : m_outCity; }
  bool IsValid() const { return m_inCity.IsValid() && m_outCity.IsValid(); }

  SpeedKMpH m_inCity;
  SpeedKMpH m_outCity;
};

struct InOutCityFactor
{
  constexpr explicit InOutCityFactor(SpeedFactor const & factor) noexcept
    : m_inCity(factor), m_outCity(factor)
  {
  }
  constexpr InOutCityFactor(SpeedFactor const & inCity, SpeedFactor const & outCity) noexcept
    : m_inCity(inCity), m_outCity(outCity)
  {
  }

  bool operator==(InOutCityFactor const & rhs) const
  {
    return m_inCity == rhs.m_inCity && m_outCity == rhs.m_outCity;
  }

  SpeedFactor const & GetFactor(bool isCity) const { return isCity ? m_inCity : m_outCity; }
  bool IsValid() const { return m_inCity.IsValid() && m_outCity.IsValid(); }

  SpeedFactor m_inCity;
  SpeedFactor m_outCity;
};

struct HighwayBasedInfo
{
  HighwayBasedInfo(HighwayBasedSpeeds const & speeds, HighwayBasedFactors const & factors)
    : m_speeds(speeds)
    , m_factors(factors)
  {
  }

  HighwayBasedSpeeds m_speeds;
  HighwayBasedFactors const & m_factors;
};

class VehicleModelInterface
{
public:
  virtual ~VehicleModelInterface() = default;

  using FeatureTypes = feature::TypesHolder;

  /// @return Allowed weight and ETA speed in KMpH.
  /// 0 means that it's forbidden to move on this feature or it's not a road at all.
  /// Weight and ETA should be less than max model speed's values respectively.
  /// @param inCity is true if |f| lies in a city of town.
  virtual SpeedKMpH GetSpeed(FeatureTypes const & types, SpeedParams const & speedParams) const = 0;

  virtual std::optional<HighwayType> GetHighwayType(FeatureTypes const & types) const = 0;

  /// @return Maximum model weight speed (km/h).
  /// All speeds which the model returns must be less or equal to this speed.
  /// @see EdgeEstimator::CalcHeuristic.
  virtual double GetMaxWeightSpeed() const = 0;

  /// @return Offroad speed in KMpH for vehicle. This speed should be used for non-feature routing
  /// e.g. to connect start point to nearest feature.
  virtual SpeedKMpH const & GetOffroadSpeed() const = 0;

  virtual bool IsOneWay(FeatureTypes const & types) const = 0;

  /// @returns true iff feature |f| can be used for routing with corresponding vehicle model.
  virtual bool IsRoad(FeatureTypes const & types) const = 0;

  /// @returns true iff feature |f| can be used for through passage with corresponding vehicle model.
  /// e.g. in Russia roads tagged "highway = service" are not allowed for through passage;
  /// however, road with this tag can be be used if it is close enough to the start or destination
  /// point of the route.
  /// Roads with additional types e.g. "path = ferry", "vehicle_type = yes" considered as allowed
  /// to pass through.
  virtual bool IsPassThroughAllowed(FeatureTypes const & types) const = 0;
};

class VehicleModelFactoryInterface
{
public:
  virtual ~VehicleModelFactoryInterface() = default;
  /// @return Default vehicle model which corresponds for all countrines,
  /// but it may be non optimal for some countries
  virtual std::shared_ptr<VehicleModelInterface> GetVehicleModel() const = 0;

  /// @return The most optimal vehicle model for specified country
  virtual std::shared_ptr<VehicleModelInterface> GetVehicleModelForCountry(
      std::string const & country) const = 0;
};

class VehicleModel : public VehicleModelInterface
{
public:
  struct FeatureTypeLimits
  {
    HighwayType m_type;
    bool m_isPassThroughAllowed;      // pass through this road type is allowed
  };

  struct FeatureTypeSurface
  {
    std::vector<std::string> m_type;  // road surface type 'psurface=*'
    SpeedFactor m_factor;
  };

  struct AdditionalRoad
  {
    std::vector<std::string> m_type;
    InOutCitySpeedKMpH m_speed;
  };

  using AdditionalRoadsList = std::initializer_list<AdditionalRoad>;
  using LimitsInitList = std::vector<FeatureTypeLimits>;
  using SurfaceInitList = std::initializer_list<FeatureTypeSurface>;

  VehicleModel(Classificator const & classif, LimitsInitList const & featureTypeLimits,
               SurfaceInitList const & featureTypeSurface, HighwayBasedInfo const & info);

  /// @name VehicleModelInterface overrides.
  /// @{
  std::optional<HighwayType> GetHighwayType(FeatureTypes const & types) const override;
  double GetMaxWeightSpeed() const override;

  /// \returns true if |types| is a oneway feature.
  /// \note According to OSM, tag "oneway" could have value "-1". That means it's a oneway feature
  /// with reversed geometry. In that case at map generation the geometry of such features
  /// is reversed (the order of points is changed) so in vehicle model all oneway feature
  /// may be considered as features with forward geometry.
  bool IsOneWay(FeatureTypes const & types) const override;
  bool IsRoad(FeatureTypes const & types) const override;
  bool IsPassThroughAllowed(FeatureTypes const & types) const override;
  /// @}

  // Made public to have simple access from unit tests.
public:
  /// @returns true if |m_highwayTypes| or |m_addRoadTypes| contains |type| and false otherwise.
  bool IsRoadType(uint32_t type) const;
  template <class TList> bool HasRoadType(TList const & types) const
  {
    for (uint32_t t : types)
    {
      if (IsRoadType(t))
        return true;
    }
    return false;
  }

  bool EqualsForTests(VehicleModel const & rhs) const
  {
    return (m_roadTypes == rhs.m_roadTypes) && (m_addRoadTypes == rhs.m_addRoadTypes) &&
           (m_onewayType == rhs.m_onewayType);
  }

protected:
  uint32_t m_yesType, m_noType;
  bool IsRoadImpl(FeatureTypes const & types) const;

  SpeedKMpH GetTypeSpeedImpl(FeatureTypes const & types, SpeedParams const & params, bool isCar) const;

  void AddAdditionalRoadTypes(Classificator const & classif, AdditionalRoadsList const & roads);

  uint32_t PrepareToMatchType(uint32_t type) const;

  /// \brief Maximum within all the speed limits set in a model (car model, bicycle model and so on).
  /// Do not mix with maxspeed value tag, which defines maximum legal speed on a feature.
  SpeedKMpH m_maxModelSpeed;

private:
  std::optional<HighwayType> GetHighwayType(uint32_t type) const;
  void GetSurfaceFactor(uint32_t type, SpeedFactor & factor) const;
  void GetAdditionalRoadSpeed(uint32_t type, bool isCityRoad,
                              std::optional<SpeedKMpH> & speed) const;

  // HW type -> speed and factor.
  HighwayBasedInfo m_highwayBasedInfo;
  uint32_t m_onewayType;

  // HW type -> allow pass through.
  base::SmallMap<uint32_t, bool> m_roadTypes;
  // Mapping surface types psurface={paved_good/paved_bad/unpaved_good/unpaved_bad} to surface speed factors.
  base::SmallMapBase<uint32_t, SpeedFactor> m_surfaceFactors;
  SpeedFactor m_minSurfaceFactorForMaxspeed;

  /// @todo Do we really need a separate map here or can merge with the m_roadTypes map?
  base::SmallMapBase<uint32_t, InOutCitySpeedKMpH> m_addRoadTypes;
};

class VehicleModelFactory : public VehicleModelFactoryInterface
{
public:
  // Callback which takes territory name (mwm graph node name) and returns its parent
  // territory name. Used to properly find local restrictions in GetVehicleModelForCountry.
  // For territories which do not have parent (countries) or have multiple parents
  // (disputed territories) it should return empty name.
  // For example "Munich" -> "Bavaria"; "Bavaria" -> "Germany"; "Germany" -> ""
  using CountryParentNameGetterFn = std::function<std::string(std::string const &)>;

  std::shared_ptr<VehicleModelInterface> GetVehicleModel() const override;

  std::shared_ptr<VehicleModelInterface> GetVehicleModelForCountry(
      std::string const & country) const override;

protected:
  explicit VehicleModelFactory(CountryParentNameGetterFn const & countryParentNameGetterFn);
  std::string GetParent(std::string const & country) const;

  std::unordered_map<std::string, std::shared_ptr<VehicleModelInterface>> m_models;
  CountryParentNameGetterFn m_countryParentNameGetterFn;
};

HighwayBasedFactors GetOneFactorsForBicycleAndPedestrianModel();

std::string DebugPrint(SpeedKMpH const & speed);
std::string DebugPrint(SpeedFactor const & speedFactor);
std::string DebugPrint(InOutCitySpeedKMpH const & speed);
std::string DebugPrint(InOutCityFactor const & speedFactor);
std::string DebugPrint(HighwayType type);
}  // namespace routing
