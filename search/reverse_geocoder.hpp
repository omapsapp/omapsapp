#pragma once

#include "indexer/house_to_street_iface.hpp"
#include "indexer/feature_decl.hpp"

#include "storage/storage_defines.hpp"

#include "coding/string_utf8_multilang.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <regex>

class FeatureType;
class DataSource;

namespace storage
{
class CountryInfoGetter;
}  // namespace storage

namespace search
{
class MwmContext;
class CityFinder;
class RegionInfoGetter;

class ReverseGeocoder
{
  DataSource const & m_dataSource;

  struct Object
  {
    FeatureID m_id;
    double m_distanceMeters;
    std::string m_name;

    Object() : m_distanceMeters(-1.0) {}
    Object(FeatureID const & id, double dist, std::string_view name)
      : m_id(id), m_distanceMeters(dist), m_name(name)
    {
    }

    inline bool IsValid() const { return m_id.IsValid(); }
  };

  friend std::string DebugPrint(Object const & obj);

public:
  /// All "Nearby" functions work in this lookup radius.
  static int constexpr kLookupRadiusM = 500;

  explicit ReverseGeocoder(DataSource const & dataSource);

  struct Street : public Object
  {
    StringUtf8Multilang m_multilangName;

    Street() = default;
    Street(FeatureID const & id, double dist, std::string_view name,
           StringUtf8Multilang const & multilangName)
      : Object(id, dist, name), m_multilangName(multilangName)
    {
    }
  };
  using Place = Street;

  struct Building : public Object
  {
    m2::PointD m_center;

    // To investigate possible errors.
    // There are no houses in (0, 0) coordinates.
    Building() : m_center(0, 0) {}

    Building(FeatureID const & id, double dist, std::string const & number,
             m2::PointD const & center)
      : Object(id, dist, number), m_center(center)
    {
    }
  };

  struct Address
  {
    Building m_building;
    Street m_street;

    std::string const GetReadableHouseNumber() const {
        std::string out;
        if (GetHouseNumber().find(':') != std::string::npos)
            // Instead of 123:456 for house number ranges, do 123 - 456 except with thin spaces and an en-dash
            out = std::regex_replace(GetHouseNumber(), std::regex("^(\\d+):(\\d+)$"), "$1 – $2");
        else
            out = GetHouseNumber();
        return (std::string const)out;
    }
    std::string const & GetHouseNumber() const { return m_building.m_name; }
    std::string const & GetStreetName() const { return m_street.m_name; }
    double GetDistance() const { return m_building.m_distanceMeters; }
    bool IsValid() const { return m_building.IsValid() && m_street.IsValid(); }

    // 7 vulica Frunze
    std::string FormatAddress() const;
  };

  struct RegionAddress
  {
    // CountryId is set in case when FeatureID is not found or belongs to World.wmw
    // and we can't get country name from it.
    storage::CountryId m_countryId;
    FeatureID m_featureId;

    bool IsValid() const;
    std::string GetCountryName() const;
    bool operator==(RegionAddress const & rhs) const;
    bool operator!=(RegionAddress const & rhs) const;
    bool operator<(RegionAddress const & rhs) const;
  };

  friend std::string DebugPrint(Address const & addr);

  /// Returns a feature id of street from |streets| whose name best matches |keyName|
  /// or empty value if the match was not found.
  static std::optional<uint32_t> GetMatchedStreetIndex(std::string_view keyName,
                                                       std::vector<Street> const & streets);

  /// @return Sorted by distance streets vector for the specified MwmId.
  /// Parameter |includeSquaresAndSuburbs| needed for backward compatibility:
  /// existing mwms operate on streets without squares and suburbs.
  static std::vector<Street> GetNearbyStreets(
      search::MwmContext & context, m2::PointD const & center, double radiusM = kLookupRadiusM);
  std::vector<Street> GetNearbyStreets(MwmSet::MwmId const & id, m2::PointD const & center) const;
  std::vector<Street> GetNearbyStreets(FeatureType & ft) const;

  static std::vector<Place> GetNearbyPlaces(
      search::MwmContext & context, m2::PointD const & center, double radiusM);

  /// @return feature street name.
  /// Returns empty string when there is no street the feature belongs to.
  std::string GetFeatureStreetName(FeatureType & ft) const;
  /// Same with GetFeatureStreetName but gets street from mwm only (not editor).
  std::string GetOriginalFeatureStreetName(FeatureID const & fid) const;
  /// For |houseId| with street information sets |streetId| to FeatureID of street corresponding to
  /// |houseId| and returns true. Returs false otherwise.
  bool GetOriginalStreetByHouse(FeatureType & house, FeatureID & streetId) const;

  /// @return The nearest exact address where building has house number and valid street match.
  void GetNearbyAddress(m2::PointD const & center, Address & addr) const;

  /// @param[in]  placeAsStreet Use house->place index same as streets to define address.
  /// Looks like it should be always true, but keep a param for now.
  /// @{
  /// @param[out] addr  The nearest exact address where building is at most |maxDistanceM| far from |center|,
  /// has house number and valid street (place) match.
  void GetNearbyAddress(m2::PointD const & center, double maxDistanceM, Address & addr,
                        bool placeAsStreet = false) const;
  /// @param[out] addr  The exact address of a feature.
  /// @returns false if  can't extruct address or ft have no house number.
  bool GetExactAddress(FeatureType & ft, Address & addr, bool placeAsStreet = false) const;
  /// @}

  bool GetExactAddress(FeatureID const & fid, Address & addr) const;

  /// Returns the nearest region address where mwm or exact city is known.
  static RegionAddress GetNearbyRegionAddress(m2::PointD const & center,
                                              storage::CountryInfoGetter const & infoGetter,
                                              CityFinder & cityFinder);
  std::string GetLocalizedRegionAddress(RegionAddress const & addr,
                                        RegionInfoGetter const & nameGetter) const;

private:
  /// Helper class to incapsulate house 2 street table reloading.
  class HouseTable
  {
  public:
    explicit HouseTable(DataSource const & dataSource, bool placeAsStreet = false)
      : m_dataSource(dataSource), m_placeAsStreet(placeAsStreet)
    {
    }
    std::optional<HouseToStreetTable::Result> Get(FeatureID const & fid);

  private:
    DataSource const & m_dataSource;
    MwmSet::MwmHandle m_handle;
    bool m_placeAsStreet;
  };

  /// Ignores changes from editor if |ignoreEdits| is true.
  bool GetNearbyAddress(HouseTable & table, Building const & bld, bool ignoreEdits,
                        Address & addr) const;

  /// @return Sorted by distance houses vector with valid house number.
  void GetNearbyBuildings(m2::PointD const & center, double maxDistanceM,
                          std::vector<Building> & buildings) const;

  static Building FromFeature(FeatureType & ft, double distMeters);
};

} // namespace search
