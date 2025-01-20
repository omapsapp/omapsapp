#include "map/place_page_info.hpp"

#include "map/bookmark_helpers.hpp"

#include "indexer/feature_utils.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/road_shields_parser.hpp"

#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/preferred_languages.hpp"
#include "platform/utm_mgrs_utils.hpp"
#include "platform/distance.hpp"
#include "platform/duration.hpp"
#include "platform/settings.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"

#include "3party/open-location-code/openlocationcode.h"


namespace place_page
{

bool Info::IsBookmark() const
{
  return BookmarkManager::IsBookmarkCategory(m_markGroupId) && BookmarkManager::IsBookmark(m_bookmarkId);
}

bool Info::ShouldShowAddPlace() const
{
  auto const isPointOrBuilding = IsPointType() || IsBuilding();
  return !(IsFeature() && isPointOrBuilding);
}

void Info::SetFromFeatureType(FeatureType & ft)
{
  MapObject::SetFromFeatureType(ft);
  m_hasMetadata = true;

  feature::NameParamsOut out;
  auto const mwmInfo = GetID().m_mwmId.GetInfo();
  if (mwmInfo)
  {
    feature::GetPreferredNames({ m_name, mwmInfo->GetRegionData(), languages::GetCurrentMapLanguage(),
                               true /* allowTranslit */} , out);
  }

  bool emptyTitle = false;

  m_primaryFeatureName = out.GetPrimary();
  m_uiAddress = m_address;

  if (IsBookmark())
  {
    m_uiTitle = GetBookmarkName();

    std::string secondaryTitle;

    if (!m_customName.empty())
      secondaryTitle = m_customName;
    else if (!out.secondary.empty())
      secondaryTitle = out.secondary;
    else
      secondaryTitle = m_primaryFeatureName;

    if (m_uiTitle != secondaryTitle)
      m_uiSecondaryTitle = std::move(secondaryTitle);
  }
  else if (!m_primaryFeatureName.empty())
  {
    m_uiTitle = m_primaryFeatureName;
    m_uiSecondaryTitle = out.secondary;
  }
  else
  {
    if (IsBuilding())
    {
      emptyTitle = m_address.empty();
      if (!emptyTitle)
        m_uiTitle = m_address;
      m_uiAddress.clear();    // already in main title
    }
    else
      emptyTitle = true;
  }

  // Assign Feature's type if main title is empty.
  if (emptyTitle)
    m_uiTitle = GetLocalizedType();

  // Append local_ref tag into main title.
  if (IsPublicTransportStop())
  {
    auto const lRef = GetMetadata(feature::Metadata::FMD_LOCAL_REF);
    if (!lRef.empty())
      m_uiTitle.append(" (").append(lRef).append(")");
  }

  m_uiSubtitle = FormatSubtitle(IsFeature() /* withTypes */, !emptyTitle /* withMainType */);

  // apply to all types after checks
  m_isHotel = ftypes::IsHotelChecker::Instance()(ft);
}

void Info::SetMercator(m2::PointD const & mercator)
{
  m_mercator = mercator;
  m_buildInfo.m_mercator = mercator;
}

std::string Info::FormatSubtitle(bool withTypes, bool withMainType) const
{
  std::string result;
  auto const append = [&result](std::string_view sv)
  {
    if (!result.empty())
      result += feature::kFieldsSeparator;
    result += sv;
  };

  if (IsBookmark())
    append(m_bookmarkCategoryName);

  if (!withTypes)
    return result;

  // Types
  append(GetLocalizedAllTypes(withMainType));

  // Flats.
  auto const flats = GetMetadata(feature::Metadata::FMD_FLATS);
  if (!flats.empty())
    append(flats);

  // Cuisines.
  for (auto const & cuisine : GetLocalizedCuisines())
    append(cuisine);

  // Recycling types.
  for (auto const & recycling : GetLocalizedRecyclingTypes())
    append(recycling);

  // Airport IATA code.
  auto const iata = GetMetadata(feature::Metadata::FMD_AIRPORT_IATA);
  if (!iata.empty())
    append(iata);

  // Road numbers/ids.
  auto const roadShields = FormatRoadShields();
  if (!roadShields.empty())
    append(roadShields);

  // Stars.
  auto const stars = feature::FormatStars(GetStars());
  if (!stars.empty())
    append(stars);

  // Operator.
  auto const op = GetMetadata(feature::Metadata::FMD_OPERATOR);
  if (!op.empty())
    append(op);

  // Brand.
  auto const brand = GetMetadata(feature::Metadata::FMD_BRAND);
  if (!brand.empty() && brand != op)
  {
    /// @todo May not work as expected because we store raw value from OSM,
    /// while current localizations assume to have some string ids (like "mcdonalds").
    auto const locBrand = platform::GetLocalizedBrandName(std::string(brand));

    // Do not duplicate for commonly used titles like McDonald's, Starbucks, etc.
    if (locBrand != m_uiTitle && locBrand != m_uiSecondaryTitle)
      append(locBrand);
  }

  // Elevation.
  auto const eleStr = feature::FormatElevation(GetMetadata(MetadataID::FMD_ELE));
  if (!eleStr.empty())
    append(eleStr);

  // ATM
  if (HasAtm())
    append(feature::kAtmSymbol);

  // Internet.
  if (HasWifi())
    append(feature::kWifiSymbol);

  // Toilets.
  if (HasToilets())
    append(feature::kToiletsSymbol);

  // Drinking Water
  auto const drinkingWater = feature::FormatDrinkingWater(GetTypes());
  if (!drinkingWater.empty())
    append(drinkingWater);

  // Wheelchair
  if (feature::GetWheelchairType(m_types) == ftraits::WheelchairAvailability::Yes)
    append(feature::kWheelchairSymbol);

  // Fee.
  auto const fee = GetLocalizedFeeType();
  if (!fee.empty())
    append(fee);

  // Debug types
  bool debugAllTypesSetting = false;
  settings::TryGet(kDebugAllTypesSetting, debugAllTypesSetting);
  if (debugAllTypesSetting)
    append(GetAllReadableTypes());

  return result;
}

std::string Info::GetBookmarkName()
{
  std::string bookmarkName;

  auto const mwmInfo = GetID().m_mwmId.GetInfo();
  if (mwmInfo)
  {
    bookmarkName = GetPreferredBookmarkStr(m_bookmarkData.m_customName, mwmInfo->GetRegionData());
    if (bookmarkName.empty())
      bookmarkName = GetPreferredBookmarkStr(m_bookmarkData.m_name, mwmInfo->GetRegionData());
  }

  if (bookmarkName.empty())
    bookmarkName = GetPreferredBookmarkName(m_bookmarkData);

  return bookmarkName;
}

void Info::SetTitlesForBookmark()
{
  m_uiTitle = GetBookmarkName();

  std::vector<std::string> subtitle;
  subtitle.push_back(m_bookmarkCategoryName);
  if (!m_bookmarkData.m_featureTypes.empty())
    subtitle.push_back(GetLocalizedFeatureType(m_bookmarkData.m_featureTypes));
  m_uiSubtitle = strings::JoinStrings(subtitle, feature::kFieldsSeparator);
}

void Info::SetCustomName(std::string const & name)
{
  if (IsBookmark())
    SetTitlesForBookmark();
  else
    m_uiTitle = name;

  m_customName = name;
}

void Info::SetTitlesForTrack(Track const & track)
{
  m_uiTitle = track.GetName();
  m_uiSubtitle = m_bookmarkCategoryName;

  std::vector<std::string> statistics;
  auto const length = track.GetLengthMeters();
  auto const duration = track.GetDurationInSeconds();
  statistics.push_back(platform::Distance::CreateFormatted(length).ToString());
  if (duration > 0)
    statistics.push_back(platform::Duration(duration).GetPlatformLocalizedString());
  m_uiTrackStatistics = strings::JoinStrings(statistics, feature::kFieldsSeparator);
}

void Info::SetCustomNames(std::string const & title, std::string const & subtitle)
{
  m_uiTitle = title;
  m_uiSubtitle = subtitle;
  m_customName = title;
}

void Info::SetCustomNameWithCoordinates(m2::PointD const & mercator, std::string const & name)
{
  if (IsBookmark())
  {
    SetTitlesForBookmark();
  }
  else
  {
    m_uiTitle = name;
    m_uiSubtitle = measurement_utils::FormatLatLon(
        mercator::YToLat(mercator.y), mercator::XToLon(mercator.x),
                                                   true /* withComma */);
  }
  m_customName = name;
}

void Info::SetFromBookmarkProperties(kml::Properties const & p)
{
  if (auto const hours = p.find("hours"); hours != p.end() && !hours->second.empty())
    m_metadata.Set(feature::Metadata::EType::FMD_OPEN_HOURS, hours->second);
  if (auto const phone = p.find("phone"); phone != p.end() && !phone->second.empty())
    m_metadata.Set(feature::Metadata::EType::FMD_PHONE_NUMBER, phone->second);
  if (auto const email = p.find("email"); email != p.end() && !email->second.empty())
    m_metadata.Set(feature::Metadata::EType::FMD_EMAIL, email->second);
  if (auto const url = p.find("url"); url != p.end() && !url->second.empty())
    m_metadata.Set(feature::Metadata::EType::FMD_WEBSITE, url->second);
  m_hasMetadata = true;
}

void Info::SetBookmarkId(kml::MarkId bookmarkId)
{
  m_bookmarkId = bookmarkId;
  m_uiSubtitle = FormatSubtitle(IsFeature() /* withTypes */, IsFeature() /* withMainType */);
}

bool Info::ShouldShowEditPlace() const
{
  // TODO(mgsergio): Does IsFeature() imply !IsMyPosition()?
  return !IsMyPosition() && IsFeature();
}

kml::LocalizableString Info::FormatNewBookmarkName() const
{
  kml::LocalizableString bookmarkName;
  if (IsFeature())
  {
    m_name.ForEach([&bookmarkName](int8_t langCode, std::string_view localName)
    {
      if (!localName.empty())
        bookmarkName[langCode] = localName;
    });

    if (bookmarkName.empty() && IsBuilding() && !m_address.empty())
      kml::SetDefaultStr(bookmarkName, m_address);
  }
  else if (!m_uiTitle.empty())
  {
    if (IsMyPosition())
      kml::SetDefaultStr(bookmarkName, platform::GetLocalizedMyPositionBookmarkName());
    else
      kml::SetDefaultStr(bookmarkName, m_uiTitle);
  }

  return bookmarkName;
}

std::string Info::GetFormattedCoordinate(CoordinatesFormat coordsFormat) const
{
  auto const & ll = GetLatLon();
  auto const lat = ll.m_lat;
  auto const lon = ll.m_lon;
  switch (coordsFormat)
  {
    default:
    case CoordinatesFormat::LatLonDMS: // DMS, comma separated
      return measurement_utils::FormatLatLonAsDMS(lat, lon, false /*withComma*/, 2);
    case CoordinatesFormat::LatLonDecimal: // Decimal, comma separated
      return measurement_utils::FormatLatLon(lat, lon, true /* withComma */);
    case CoordinatesFormat::OLCFull: // Open location code, long format
      return openlocationcode::Encode({lat, lon});
    case CoordinatesFormat::OSMLink: // Link to osm.org
      return measurement_utils::FormatOsmLink(lat, lon, 14);
    case CoordinatesFormat::UTM:  // Universal Transverse Mercator
    {
      std::string utmCoords = utm_mgrs_utils::FormatUTM(lat, lon);
      if (utmCoords.empty())
        return "UTM: N/A";
      else
        return "UTM: " + utmCoords;
    }
    case CoordinatesFormat::MGRS: // Military Grid Reference System
    {
      std::string mgrsCoords = utm_mgrs_utils::FormatMGRS(lat, lon, 5);
      if (mgrsCoords.empty())
        return "MGRS: N/A";
      else
        return "MGRS: " + mgrsCoords;
    }
  }
}

void Info::SetRoadType(RoadWarningMarkType type, std::string const & localizedType, std::string const & distance)
{
  m_roadType = type;
  m_uiTitle = localizedType;
  m_uiSubtitle = distance;
}

void Info::SetRoadType(FeatureType & ft, RoadWarningMarkType type, std::string const & localizedType,
                       std::string const & distance)
{
  auto const addTitle = [this](std::string && str)
  {
    if (!m_uiTitle.empty())
    {
      m_uiTitle += feature::kFieldsSeparator;
      m_uiTitle += str;
    }
    else
      m_uiTitle = std::move(str);
  };

  auto const addSubtitle = [this](std::string_view sv)
  {
    if (!m_uiSubtitle.empty())
      m_uiSubtitle += feature::kFieldsSeparator;
    m_uiSubtitle += sv;
  };

  CHECK_NOT_EQUAL(type, RoadWarningMarkType::Count, ());
  m_roadType = type;

  std::vector<std::string> subtitle;
  if (type == RoadWarningMarkType::Toll)
  {
    std::vector<std::string> title;
    for (auto const & shield : ftypes::GetRoadShields(ft))
    {
      auto name = shield.m_name;
      if (!shield.m_additionalText.empty())
        name += " " + shield.m_additionalText;
      addTitle(std::move(name));
    }

    if (m_uiTitle.empty())
      m_uiTitle = m_primaryFeatureName;

    if (m_uiTitle.empty())
      m_uiTitle = localizedType;
    else
      addSubtitle(localizedType);
    addSubtitle(distance);
  }
  else if (type == RoadWarningMarkType::Dirty)
  {
    m_uiTitle = localizedType;
    addSubtitle(distance);
  }
  else if (type == RoadWarningMarkType::Ferry)
  {
    m_uiTitle = m_primaryFeatureName;
    addSubtitle(localizedType);

    auto const operatorName = GetMetadata(feature::Metadata::FMD_OPERATOR);
    if (!operatorName.empty())
      addSubtitle(operatorName);
  }
}

}  // namespace place_page
