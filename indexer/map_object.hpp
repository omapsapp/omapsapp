#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/ftraits.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/stl_helpers.hpp"

#include <string>
#include <vector>

class FeatureType;

namespace osm
{
class EditableMapObject;
/// OSM internet_access tag values.
enum class Internet
{
  Unknown,  //!< Internet state is unknown (default).
  Wlan,     //!< Wireless Internet access is present.
  Wired,    //!< Wired Internet access is present.
  Yes,      //!< Unspecified Internet access is available.
  No        //!< There is definitely no any Internet access.
};
std::string DebugPrint(Internet internet);

// Object details in the sorted order, visible to users.
// Must correspond MapObject.java
enum class Props : uint8_t
{
  OpeningHours = 0,
  Phone = 1,
  Fax = 2,
  Website = 3,
  Email = 4,
  Cuisine = 5,
  Stars = 6,
  Operator = 7,
  Elevation = 8,
  Internet = 9,
  Wikipedia = 10,
  Flats = 11,
  BuildingLevels = 12,
  Level = 13,
  FacebookPage = 14,
  InstagramPage = 15,
  TwitterPage = 16,
  VkPage = 17
};
std::string DebugPrint(Props props);

class MapObject
{
public:
  void SetFromFeatureType(FeatureType & ft);

  FeatureID const & GetID() const;

  ms::LatLon GetLatLon() const;
  m2::PointD const & GetMercator() const;
  std::vector<m2::PointD> const & GetTriangesAsPoints() const;
  std::vector<m2::PointD> const & GetPoints() const;

  feature::TypesHolder const & GetTypes() const;
  std::string GetDefaultName() const;
  StringUtf8Multilang const & GetNameMultilang() const;

  std::string const & GetHouseNumber() const;

  /// @name Metadata fields.
  //@{
  std::vector<Props> AvailableProperties() const;
  std::string GetPhone() const;
  std::string GetFax() const;
  std::string GetEmail() const;
  std::string GetWebsite() const;
  std::string GetFacebookPage() const;
  std::string GetInstagramPage() const;
  std::string GetTwitterPage() const;
  std::string GetVkPage() const;
  Internet GetInternet() const;
  /// @returns non-localized cuisines keys.
  std::vector<std::string> GetCuisines() const;
  /// @returns translated cuisine(s).
  std::vector<std::string> GetLocalizedCuisines() const;
  /// @returns non-localized recycling type(s).
  std::vector<std::string> GetRecyclingTypes() const;
  /// @returns translated recycling type(s).
  std::vector<std::string> GetLocalizedRecyclingTypes() const;
  /// @returns translated and formatted cuisines.
  std::string FormatCuisines() const;
  std::vector<std::string> GetRoadShields() const;
  std::string FormatRoadShields() const;
  std::string GetOpeningHours() const;
  std::string GetOperator() const;
  int GetStars() const;
  /// @returns formatted elevation in feet or meters, or empty string.
  std::string GetElevationFormatted() const;
  bool GetElevation(double & outElevationInMeters) const;
  /// @returns URL to Wikipedia or empty string.
  std::string GetWikipediaLink() const;
  std::string GetFlats() const;
  std::string GetBuildingLevels() const;
  std::string GetLevel() const;
  ftraits::WheelchairAvailability GetWheelchairType() const;
  std::string GetAirportIata() const;

  // TODO(Vlad, yunikkk): Use Props enum + getters instead of direct metadata access.
  // TODO: Remove this method.
  feature::Metadata const & GetMetadata() const;

  bool IsPointType() const;
  feature::GeomType GetGeomType() const { return m_geomType; };
  /// @returns true if object is of building type.
  bool IsBuilding() const;

protected:
  /// @returns "the best" type to display in UI.
  std::string GetLocalizedType() const;
  
  FeatureID m_featureID;
  m2::PointD m_mercator;

  std::vector<m2::PointD> m_points;
  std::vector<m2::PointD> m_triangles;

  StringUtf8Multilang m_name;
  std::string m_houseNumber;
  std::string m_roadNumber;
  feature::TypesHolder m_types;
  feature::Metadata m_metadata;

  feature::GeomType m_geomType = feature::GeomType::Undefined;
};

/// Helper to convert internal feature::Metadata::FMD_* enum into a users-visible one.
template <class T>
std::vector<Props> MetadataToProps(std::vector<T> const & metadata)
{
  std::vector<Props> res;
  using feature::Metadata;
  for (auto const type : metadata)
  {
    switch (static_cast<Metadata::EType>(type))
    {
    case Metadata::FMD_OPEN_HOURS: res.push_back(Props::OpeningHours); break;
    case Metadata::FMD_PHONE_NUMBER: res.push_back(Props::Phone); break;
    case Metadata::FMD_FAX_NUMBER: res.push_back(Props::Fax); break;
    case Metadata::FMD_STARS: res.push_back(Props::Stars); break;
    case Metadata::FMD_OPERATOR:
      res.push_back(Props::Operator);
      break;
    // Url is not used in UI and should be matched to Website.
    case Metadata::FMD_URL:
    case Metadata::FMD_WEBSITE: res.push_back(Props::Website); break;
    case Metadata::FMD_FACEBOOK_PAGE: res.push_back(Props::FacebookPage); break;
    case Metadata::FMD_INSTAGRAM_PAGE: res.push_back(Props::InstagramPage); break;
    case Metadata::FMD_TWITTER_PAGE: res.push_back(Props::TwitterPage); break;
    case Metadata::FMD_VK_PAGE: res.push_back(Props::VkPage); break;
    case Metadata::FMD_INTERNET: res.push_back(Props::Internet); break;
    case Metadata::FMD_ELE: res.push_back(Props::Elevation); break;
    case Metadata::FMD_EMAIL: res.push_back(Props::Email); break;
    case Metadata::FMD_WIKIPEDIA: res.push_back(Props::Wikipedia); break;
    case Metadata::FMD_FLATS: res.push_back(Props::Flats); break;
    case Metadata::FMD_BUILDING_LEVELS: res.push_back(Props::BuildingLevels); break;
    case Metadata::FMD_LEVEL: res.push_back(Props::Level); break;
    // Cuisines should be processed sepatately since release 10.0.
    case Metadata::FMD_CUISINE:
    case Metadata::FMD_TURN_LANES:
    case Metadata::FMD_TURN_LANES_FORWARD:
    case Metadata::FMD_TURN_LANES_BACKWARD:
    // Postcode should be processed separately, in the address.
    case Metadata::FMD_POSTCODE:
    case Metadata::FMD_HEIGHT:
    case Metadata::FMD_MIN_HEIGHT:
    case Metadata::FMD_DENOMINATION:
    case Metadata::FMD_TEST_ID:
    case Metadata::FMD_SPONSORED_ID:
    case Metadata::FMD_PRICE_RATE:
    case Metadata::FMD_RATING:
    case Metadata::FMD_BANNER_URL:
    case Metadata::FMD_AIRPORT_IATA:
    case Metadata::FMD_BRAND:
    case Metadata::FMD_DURATION:
    case Metadata::FMD_COUNT:
      break;
      // Please add new cases when compiler issues an "unhandled switch case" warning here.
    }
  }
  base::SortUnique(res);
  return res;
}
}  // namespace osm
