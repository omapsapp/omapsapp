#pragma once

#include "map/routing_mark.hpp"

#include "storage/storage_defines.hpp"

#include "drape_frontend/frontend_renderer.hpp"
#include "drape_frontend/selection_shape.hpp"

#include "kml/types.hpp"

#include "indexer/feature_data.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/map_object.hpp"

#include "geometry/point2d.hpp"

#include <string>
#include <vector>

namespace place_page
{
enum class OpeningMode
{
  Preview = 0,
  PreviewPlus,
  Details,
  Full
};

enum class CoordinatesFormat
{
  LatLonDMS = 0, // DMS, comma separated
  LatLonDecimal, // Decimal, comma separated
  OLCFull, // Open location code, long format
  OSMLink, // Link to osm.org
  UTM, // Universal Transverse Mercator
  MGRS // Military Grid Reference System
};

struct BuildInfo
{
  enum class Source : uint8_t
  {
    User,
    Search,
    Other
  };

  enum class Match : uint8_t
  {
    Everything = 0,
    FeatureOnly,
    UserMarkOnly,
    TrackOnly,
    Nothing
  };

  BuildInfo() = default;

  explicit BuildInfo(df::TapInfo const & info)
    : m_source(Source::User)
    , m_mercator(info.m_mercator)
    , m_isLongTap(info.m_isLong)
    , m_isMyPosition(info.m_isMyPositionTapped)
    , m_featureId(info.m_featureTapped)
    , m_userMarkId(info.m_markTapped)
  {}

  bool IsFeatureMatchingEnabled() const
  {
    return m_match == Match::Everything || m_match == Match::FeatureOnly;
  }

  bool IsUserMarkMatchingEnabled() const
  {
    return m_match == Match::Everything || m_match == Match::UserMarkOnly;
  }

  bool IsTrackMatchingEnabled() const
  {
    return m_match == Match::Everything || m_match == Match::TrackOnly;
  }

  Source m_source = Source::Other;
  m2::PointD m_mercator;
  bool m_isLongTap = false;
  bool m_isMyPosition = false;
  FeatureID m_featureId;
  Match m_match = Match::Everything;
  kml::MarkId m_userMarkId = kml::kInvalidMarkId;
  kml::TrackId m_trackId = kml::kInvalidTrackId;
  bool m_isGeometrySelectionAllowed = false;
  bool m_needAnimationOnSelection = true;
  std::string m_postcode;
};

class Info : public osm::MapObject
{
public:

  void SetBuildInfo(place_page::BuildInfo const & info) { m_buildInfo = info; }
  place_page::BuildInfo const & GetBuildInfo() const { return m_buildInfo; }

  /// Place traits
  bool IsFeature() const { return m_featureID.IsValid(); }
  bool IsBookmark() const;
  bool IsTrack() const { return m_trackId != kml::kInvalidTrackId; }
  bool IsMyPosition() const { return m_selectedObject == df::SelectionShape::ESelectedObject::OBJECT_MY_POSITION; }
  bool IsRoutePoint() const { return m_isRoutePoint; }
  bool IsRoadType() const { return m_roadType != RoadWarningMarkType::Count; }
  bool HasMetadata() const { return m_hasMetadata; }

  /// Edit and add
  bool ShouldShowAddPlace() const;
  bool ShouldShowAddBusiness() const { return m_canEditOrAdd && IsBuilding(); }
  bool ShouldShowEditPlace() const;

  bool ShouldEnableAddPlace() const { return m_canEditOrAdd; };
  bool ShouldEnableEditPlace() const { return m_canEditOrAdd; };

  /// @returns true if Back API button should be displayed.
  bool HasApiUrl() const { return !m_apiUrl.empty(); }
  /// TODO: Support all possible Internet types in UI. @See MapObject::GetInternet().
  bool HasWifi() const { return GetInternet() == feature::Internet::Wlan; }
  /// Should be used by UI code to generate cool name for new bookmarks.
  // TODO: Tune new bookmark name. May be add address or some other data.
  kml::LocalizableString FormatNewBookmarkName() const;

  /// For showing in UI
  std::string const & GetTitle() const { return m_uiTitle; };
  /// Convenient wrapper for secondary feature name.
  std::string const & GetSecondaryTitle() const { return m_uiSecondaryTitle; };
  /// Convenient wrapper for type, cuisines, elevation, stars, wifi etc.
  std::string const & GetSubtitle() const { return m_uiSubtitle; };
  std::string const & GetSecondarySubtitle() const { return !m_uiTrackStatistics.empty() ? m_uiTrackStatistics : m_uiAddress; };
  std::string const & GetWikiDescription() const { return m_description; }
  /// @returns coordinate in DMS format if isDMS is true
  std::string GetFormattedCoordinate(CoordinatesFormat format) const;

  /// UI setters
  void SetCustomName(std::string const & name);
  void SetTitlesForBookmark();
  void SetTitlesForTrack(Track const & track);
  void SetCustomNames(std::string const & title, std::string const & subtitle);
  void SetCustomNameWithCoordinates(m2::PointD const & mercator, std::string const & name);
  void SetAddress(std::string && address) { m_address = std::move(address); }
  void SetCanEditOrAdd(bool canEditOrAdd) { m_canEditOrAdd = canEditOrAdd; }

  /// Bookmark
  void SetFromBookmarkProperties(kml::Properties const & p);
  void SetBookmarkId(kml::MarkId bookmarkId);
  kml::MarkId GetBookmarkId() const { return m_bookmarkId; }
  void SetBookmarkCategoryId(kml::MarkGroupId markGroupId) { m_markGroupId = markGroupId; }
  kml::MarkGroupId GetBookmarkCategoryId() const { return m_markGroupId; }
  std::string const & GetBookmarkCategoryName() const { return m_bookmarkCategoryName; }
  void SetBookmarkCategoryName(std::string const & name) { m_bookmarkCategoryName = name; }
  void SetBookmarkData(kml::BookmarkData const & data) { m_bookmarkData = data; }
  kml::BookmarkData const & GetBookmarkData() const { return m_bookmarkData; }

  /// Track
  void SetTrackId(kml::TrackId trackId) { m_trackId = trackId; };
  kml::TrackId GetTrackId() const { return m_trackId; };

  /// Api
  void SetApiId(std::string const & apiId) { m_apiId = apiId; }
  void SetApiUrl(std::string const & url) { m_apiUrl = url; }
  std::string const & GetApiUrl() const { return m_apiUrl; }

  void SetOpeningMode(OpeningMode openingMode) { m_openingMode = openingMode; }
  OpeningMode GetOpeningMode() const { return m_openingMode; }

  /// Feature status
  void SetFeatureStatus(FeatureStatus const status) { m_featureStatus = status; }
  FeatureStatus GetFeatureStatus() const { return m_featureStatus; }

  /// Routing
  void SetRouteMarkType(RouteMarkType type) { m_routeMarkType = type; }
  RouteMarkType GetRouteMarkType() const { return m_routeMarkType; }
  void SetIntermediateIndex(size_t index) { m_intermediateIndex = index; }
  size_t GetIntermediateIndex() const { return m_intermediateIndex; }
  void SetIsRoutePoint() { m_isRoutePoint = true; }

  /// Road type
  void SetRoadType(FeatureType & ft, RoadWarningMarkType type, std::string const & localizedType,
                   std::string const & distance);
  void SetRoadType(RoadWarningMarkType type, std::string const & localizedType, std::string const & distance);
  RoadWarningMarkType GetRoadType() const { return m_roadType; }

  /// CountryId
  /// Which mwm this MapObject is in.
  /// Exception: for a country-name point it will be set to the topmost node for the mwm.
  /// TODO(@a): use m_topmostCountryIds in exceptional case.
  void SetCountryId(storage::CountryId const & countryId) { m_countryId = countryId; }
  storage::CountryId const & GetCountryId() const { return m_countryId; }
  template <typename Countries>
  void SetTopmostCountryIds(Countries && ids)
  {
    m_topmostCountryIds = std::forward<Countries>(ids);
  }
  storage::CountriesVec const & GetTopmostCountryIds() const { return m_topmostCountryIds; }

  /// MapObject
  void SetFromFeatureType(FeatureType & ft);

  void SetWikiDescription(std::string && description) { m_description = std::move(description); }

  void SetMercator(m2::PointD const & mercator);
  std::vector<std::string> GetRawTypes() const { return m_types.ToObjectNames(); }

  bool IsHotel() const { return m_isHotel; }

  // void SetPopularity(uint8_t popularity) { m_popularity = popularity; }
  // uint8_t GetPopularity() const { return m_popularity; }
  std::string const & GetPrimaryFeatureName() const { return m_primaryFeatureName; }

  void SetSelectedObject(df::SelectionShape::ESelectedObject selectedObject) { m_selectedObject = selectedObject; }
  df::SelectionShape::ESelectedObject GetSelectedObject() const { return m_selectedObject; }

private:
  std::string FormatSubtitle(bool withTypes, bool withMainType) const;
  std::string GetBookmarkName();

  place_page::BuildInfo m_buildInfo;

  /// UI
  std::string m_uiTitle;
  std::string m_uiSubtitle;
  std::string m_uiSecondaryTitle;
  std::string m_uiAddress;
  std::string m_uiTrackStatistics;
  std::string m_description;
  /// Booking rating string
  std::string m_localizedRatingString;

  /// CountryId
  storage::CountryId m_countryId = storage::kInvalidCountryId;
  /// The topmost downloader nodes this MapObject is in, i.e.
  /// the country name for an object whose mwm represents only
  /// one part of the country (or several countries for disputed territories).
  storage::CountriesVec m_topmostCountryIds;

  /// Comes from API, shared links etc.
  std::string m_customName;

  /// Bookmark or track
  kml::MarkGroupId m_markGroupId = kml::kInvalidMarkGroupId;
  /// If not invalid, bookmark is bound to this place page.
  kml::MarkId m_bookmarkId = kml::kInvalidMarkId;
  /// Bookmark category name. Empty, if it's not bookmark;
  std::string m_bookmarkCategoryName;
  kml::BookmarkData m_bookmarkData;
  /// If not invalid, track is bound to this place page.
  kml::TrackId m_trackId = kml::kInvalidTrackId;
  /// Whether to treat it as plain feature.
  bool m_hasMetadata = false;

  /// Api ID passed for the selected object. It's automatically included in api url below.
  std::string m_apiId;
  /// [Deep] link to open when "Back" button is pressed in a Place Page.
  std::string m_apiUrl;
  /// Formatted feature address for inner using.
  std::string m_address;

  /// Routing
  RouteMarkType m_routeMarkType;
  size_t m_intermediateIndex = 0;
  bool m_isRoutePoint = false;

  /// Road type
  RoadWarningMarkType m_roadType = RoadWarningMarkType::Count;

  /// Editor
  /// True if editing of a selected point is allowed by basic logic.
  /// See initialization in framework.
  bool m_canEditOrAdd = false;

  /// Feature status
  FeatureStatus m_featureStatus = FeatureStatus::Untouched;

  bool m_isHotel = false;

  //uint8_t m_popularity = 0;

  std::string m_primaryFeatureName;

  OpeningMode m_openingMode = OpeningMode::Preview;

  df::SelectionShape::ESelectedObject m_selectedObject = df::SelectionShape::ESelectedObject::OBJECT_EMPTY;
};
}  // namespace place_page
