#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/feature_utils.hpp"

#include "base/small_map.hpp"
#include "base/stl_helpers.hpp"

#include <functional>
#include <string>
#include <vector>

#define DECLARE_CHECKER_INSTANCE(CheckerType) static CheckerType const & Instance() { \
                                              static CheckerType const inst; return inst; }

namespace ftypes
{
class BaseChecker
{
protected:
  uint8_t const m_level;
  std::vector<uint32_t> m_types;

  explicit BaseChecker(uint8_t level = 2) : m_level(level) {}
  virtual ~BaseChecker() = default;

public:
  virtual bool IsMatched(uint32_t type) const;
  virtual void ForEachType(std::function<void(uint32_t)> const & fn) const;

  std::vector<uint32_t> const & GetTypes() const { return m_types; }

  bool operator()(feature::TypesHolder const & types) const;
  bool operator()(FeatureType & ft) const;
  bool operator()(std::vector<uint32_t> const & types) const;
  bool operator()(uint32_t type) const { return IsMatched(type); }

  static uint32_t PrepareToMatch(uint32_t type, uint8_t level);
};

class IsPeakChecker : public BaseChecker
{
  IsPeakChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsPeakChecker);
};

class IsATMChecker : public BaseChecker
{
  IsATMChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsATMChecker);
};

class IsSpeedCamChecker : public BaseChecker
{
  IsSpeedCamChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsSpeedCamChecker);
};

class IsPostBoxChecker : public BaseChecker
{
  IsPostBoxChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsPostBoxChecker);
};

class IsPostPoiChecker : public BaseChecker
{
  IsPostPoiChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsPostPoiChecker);
};

class IsOperatorOthersPoiChecker : public BaseChecker
{
  IsOperatorOthersPoiChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsOperatorOthersPoiChecker);
};

class IsRecyclingCentreChecker : public BaseChecker
{
  IsRecyclingCentreChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsRecyclingCentreChecker);

  uint32_t GetType() const;
};

class IsRecyclingContainerChecker : public BaseChecker
{
  IsRecyclingContainerChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsRecyclingContainerChecker);

  uint32_t GetType() const;
};

class IsRailwayStationChecker : public BaseChecker
{
  IsRailwayStationChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsRailwayStationChecker);
};

class IsSubwayStationChecker : public BaseChecker
{
  IsSubwayStationChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsSubwayStationChecker);
};

class IsAirportChecker : public BaseChecker
{
  IsAirportChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsAirportChecker);
};

class IsSquareChecker : public BaseChecker
{
  IsSquareChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsSquareChecker);
};

// Type of suburb.
// Do not change values and order - they are in the order of decreasing specificity.
// Suburb > Neighbourhood > Residential
enum class SuburbType
{
  Residential = 0,
  Neighbourhood,
  Quarter,
  Suburb,
  Count
};

class IsSuburbChecker : public BaseChecker
{
  IsSuburbChecker();

public:
  SuburbType GetType(uint32_t t) const;
  SuburbType GetType(feature::TypesHolder const & types) const;
  SuburbType GetType(FeatureType & f) const;

  DECLARE_CHECKER_INSTANCE(IsSuburbChecker);
};

/// @todo Better to rename like IsStreetChecker, as it is used in search context only?
class IsWayChecker : public BaseChecker
{
  IsWayChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsWayChecker);

  enum SearchRank : uint8_t
  {
    Default = 0,  // Not a road, other linear way like river, rail ...

    // Bigger is better (more important).
    Pedestrian,
    Cycleway,
    Outdoor,
    Minors,
    Residential,
    Regular,
    Motorway,

    Count
  };

  SearchRank GetSearchRank(uint32_t type) const;

private:
  base::SmallMap<uint32_t, SearchRank> m_ranks;
};

class IsStreetOrSquareChecker : public BaseChecker
{
  IsStreetOrSquareChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsStreetOrSquareChecker);
};

class IsAddressObjectChecker : public BaseChecker
{
  IsAddressObjectChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsAddressObjectChecker);
};

class IsAddressChecker : public BaseChecker
{
  IsAddressChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsAddressChecker);
};

class IsVillageChecker : public BaseChecker
{
  IsVillageChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsVillageChecker);
};

class IsOneWayChecker : public BaseChecker
{
  IsOneWayChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsOneWayChecker);

  uint32_t GetType() const { return m_types[0]; }
};

class IsRoundAboutChecker : public BaseChecker
{
  IsRoundAboutChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsRoundAboutChecker);
};

class IsLinkChecker : public BaseChecker
{
  IsLinkChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsLinkChecker);
};

class IsBuildingChecker : public BaseChecker
{
  IsBuildingChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsBuildingChecker);
};

class IsBuildingPartChecker : public ftypes::BaseChecker
{
  IsBuildingPartChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsBuildingPartChecker);
};

class IsBuildingHasPartsChecker : public ftypes::BaseChecker
{
  IsBuildingHasPartsChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsBuildingHasPartsChecker);

  uint32_t GetType() const { return m_types[0]; }
};

class IsIsolineChecker : public BaseChecker
{
  IsIsolineChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsIsolineChecker);
};

class IsPisteChecker : public BaseChecker
{
  IsPisteChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsPisteChecker);
};


class OneLevelPOIChecker : public ftypes::BaseChecker
{
public:
  OneLevelPOIChecker();
};

/// Describes 2-level POI-exception types that don't belong to any POI-common classes
/// (amenity, shop, tourism, ...). Used in search algo and search categories index generation.
class TwoLevelPOIChecker : public ftypes::BaseChecker
{
public:
  TwoLevelPOIChecker();
};

// Used in:
// - search ranking (POIs are ranked somewhat higher than "unclassified" features, see search/model.hpp)
// - add type's translation into complex PP subtitle (see GetLocalizedAllTypes())
// - building lists of popular places (generator/popular_places_section_builder.cpp)
class IsPoiChecker
{
public:
  DECLARE_CHECKER_INSTANCE(IsPoiChecker);

  bool operator()(FeatureType & ft) const { return m_oneLevel(ft) || m_twoLevel(ft); }
  template <class T> bool operator()(T const & t) const { return m_oneLevel(t) || m_twoLevel(t); }

private:
  OneLevelPOIChecker const m_oneLevel;
  TwoLevelPOIChecker const m_twoLevel;
};


class IsAmenityChecker : public BaseChecker
{
  IsAmenityChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsAmenityChecker);

  uint32_t GetType() const { return m_types[0]; }
};

class AttractionsChecker : public BaseChecker
{
  size_t m_additionalTypesStart;

  AttractionsChecker();
public:
  DECLARE_CHECKER_INSTANCE(AttractionsChecker);

  // Used in generator.
  uint32_t GetBestType(FeatureParams::Types const & types) const;
};

class IsPlaceChecker : public BaseChecker
{
  IsPlaceChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsPlaceChecker);
};

class IsBridgeOrTunnelChecker : public BaseChecker
{
  virtual bool IsMatched(uint32_t type) const override;

  IsBridgeOrTunnelChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsBridgeOrTunnelChecker);
};

class IsIslandChecker : public BaseChecker
{
  IsIslandChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsIslandChecker);
};

class IsLandChecker : public BaseChecker
{
  IsLandChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsLandChecker);

  uint32_t GetLandType() const;
};

class IsCoastlineChecker : public BaseChecker
{
  IsCoastlineChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsCoastlineChecker);

  uint32_t GetCoastlineType() const;
};

class IsHotelChecker : public BaseChecker
{
  IsHotelChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsHotelChecker);
};

// WiFi is a type in classificator.txt,
// it should be checked for filling metadata in MapObject.
class IsWifiChecker : public BaseChecker
{
  IsWifiChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsWifiChecker);

  uint32_t GetType() const { return m_types[0]; }
};

class IsEatChecker : public BaseChecker
{
public:
//  enum class Type
//  {
//    Cafe = 0,
//    FastFood,
//    Restaurant,
//    Bar,
//    Pub,
//    Biergarten,

//    Count
//  };

  DECLARE_CHECKER_INSTANCE(IsEatChecker);

//  Type GetType(uint32_t t) const;

private:
  IsEatChecker();

//  std::array<uint32_t, base::Underlying(Type::Count)> m_eat2clType;
};

class IsCuisineChecker : public BaseChecker
{
  IsCuisineChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsCuisineChecker);
};

class IsRecyclingTypeChecker : public BaseChecker
{
  IsRecyclingTypeChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsRecyclingTypeChecker);
};

class IsFeeTypeChecker : public BaseChecker
{
  IsFeeTypeChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsFeeTypeChecker);
};

class IsToiletsChecker : public BaseChecker
{
  IsToiletsChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsToiletsChecker);
};

class IsCapitalChecker : public BaseChecker
{
  IsCapitalChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsCapitalChecker);
};

class IsPublicTransportStopChecker : public BaseChecker
{
  IsPublicTransportStopChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsPublicTransportStopChecker);
};

class IsTaxiChecker : public BaseChecker
{
  IsTaxiChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsTaxiChecker);
};

class IsMotorwayJunctionChecker : public BaseChecker
{
  IsMotorwayJunctionChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsMotorwayJunctionChecker);
};

/// Exotic OSM ways that potentially have "duration" tag.
class IsWayWithDurationChecker : public BaseChecker
{
  IsWayWithDurationChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsWayWithDurationChecker);
};

/// Type of locality (do not change values and order - they have detalization order)
/// Country < State < City < ...
enum class LocalityType : int8_t
{
  None = -1,
  Country = 0,
  State,
  City,
  Town,
  Village,
  Count
};

LocalityType LocalityFromString(std::string_view s);

static_assert(base::Underlying(LocalityType::Country) < base::Underlying(LocalityType::State), "");
static_assert(base::Underlying(LocalityType::State) < base::Underlying(LocalityType::City), "");
static_assert(base::Underlying(LocalityType::City) < base::Underlying(LocalityType::Town), "");
static_assert(base::Underlying(LocalityType::Town) < base::Underlying(LocalityType::Village), "");
static_assert(base::Underlying(LocalityType::Village) < base::Underlying(LocalityType::Count), "");

class IsLocalityChecker : public BaseChecker
{
  IsLocalityChecker();

public:
  LocalityType GetType(uint32_t t) const;

  template <class Types> LocalityType GetType(Types const & types) const
  {
    for (uint32_t const t : types)
    {
      LocalityType const type = GetType(t);
      if (type != LocalityType::None)
        return type;
    }
    return LocalityType::None;
  }

  LocalityType GetType(FeatureType & f) const;

  DECLARE_CHECKER_INSTANCE(IsLocalityChecker);
};


class IsCountryChecker : public BaseChecker
{
  IsCountryChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsCountryChecker);
};

class IsStateChecker : public BaseChecker
{
  IsStateChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsStateChecker);
};

class IsCityTownOrVillageChecker : public BaseChecker
{
  IsCityTownOrVillageChecker();
public:
  DECLARE_CHECKER_INSTANCE(IsCityTownOrVillageChecker);
};

class IsEntranceChecker : public BaseChecker
{
  IsEntranceChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsEntranceChecker);
};

class IsAerowayGateChecker : public BaseChecker
{
  IsAerowayGateChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsAerowayGateChecker);
};

class IsRailwaySubwayEntranceChecker : public BaseChecker
{
  IsRailwaySubwayEntranceChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsRailwaySubwayEntranceChecker);
};

class IsPlatformChecker : public BaseChecker
{
  IsPlatformChecker();

public:
  DECLARE_CHECKER_INSTANCE(IsPlatformChecker);
};

class IsAddressInterpolChecker : public BaseChecker
{
  IsAddressInterpolChecker();

  uint32_t m_odd, m_even;

public:
  DECLARE_CHECKER_INSTANCE(IsAddressInterpolChecker);

  template <class Range> feature::InterpolType GetInterpolType(Range const & range) const
  {
    for (uint32_t t : range)
    {
      if (t == m_odd)
        return feature::InterpolType::Odd;
      if (t == m_even)
        return feature::InterpolType::Even;

      ftype::TruncValue(t, 1);
      if (t == m_types[0])
        return feature::InterpolType::Any;
    }

    return feature::InterpolType::None;
  }

  feature::InterpolType GetInterpolType(FeatureType & ft) const
  {
    return GetInterpolType(feature::TypesHolder(ft));
  }
};


/// @name Get city radius and population.
/// @param r Radius in meters.
//@{
uint64_t GetDefPopulation(LocalityType localityType);
uint64_t GetPopulation(FeatureType & ft);
double GetRadiusByPopulation(uint64_t p);
double GetRadiusByPopulationForRouting(uint64_t p, LocalityType localityType);
uint64_t GetPopulationByRadius(double r);
//@}

// Highway class. The order is important.
// The enum values follow from the biggest roads (Trunk) to the smallest ones (Service).
enum class HighwayClass
{
  Undefined = 0,  // There has not been any attempt of calculating HighwayClass.
  Trunk,
  Primary,
  Secondary,
  Tertiary,
  LivingStreet,
  Service,
  Pedestrian,
  Transported,    // Vehicles are transported by train or ferry.
  Count           // This value is used for internals only.
};

std::string DebugPrint(HighwayClass const cls);
std::string DebugPrint(LocalityType const localityType);

HighwayClass GetHighwayClass(feature::TypesHolder const & types);
}  // namespace ftypes
